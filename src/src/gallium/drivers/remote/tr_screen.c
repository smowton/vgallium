/**************************************************************************
 *
 * Copyright 2008 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

#include <xen/sys/gntmem.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

#include "pipe/p_inlines.h"
#include "pipe/p_defines.h"
#include "util/u_memory.h"

#include "remote_state.h"
#include "remote_util.h"
#include "remote_comms.h"
#include "remote_messages.h"
#include "remote_debug.h"
#include "tr_screen.h"
#include "tr_winsys.h"

/* A few of the objects dealt with here are refcounted. Of course, the remote side is refcounting too. Since functions
   like pipe_texture_reference and so on in some cases just adjust the refcount variable without making a callback,
   I can't be sure of always letting the remote side know about references.
   
   My solution:
   
   * Each proxy object (e.g. remote_texture) corresponds to exactly one remote reference.
   * The proxies themselves are refcounted. So, when a proxy dies, the remote object gets a single deref.
   
   i.e. the objects in this address space are being refcounted, and their existence corresponds to a single
   reference in the remote address space.
   
*/

int analyse_waits = 0;
// A global to indicate whether we desire print statements reporting on how and why we waited for remote action

static const char *
remote_screen_get_name(struct pipe_screen *_screen)
{

    return "Xen virtual 3D\n";

}


static const char *
remote_screen_get_vendor(struct pipe_screen *_screen)
{

    return "Chris Smowton\n";

}


static int 
remote_screen_get_param(struct pipe_screen *_screen, 
                       int param) {

  struct remote_screen* rscreen = (struct remote_screen*)_screen;

  if(rscreen->int_param_is_cached[param])
    return rscreen->cached_int_params[param];

  ALLOC_OUT_MESSAGE(get_param, message);
    
  message->base.opcode = REMREQ_GET_PARAM;
  message->screen = SCREEN_HANDLE(_screen);
  message->param = param;
  
  if(analyse_waits)
    printf("WAIT: get_param\n");
  QUEUE_AND_WAIT(message, get_param, reply);
    
  DBG("Get integer parameter %d: value is %d\n", param, reply->response);
  
  rscreen->cached_int_params[param] = reply->response;
  rscreen->int_param_is_cached[param] = 1;
  int response = reply->response;
  free_message(reply);
  return response;

}


static float 
remote_screen_get_paramf(struct pipe_screen *_screen, 
                        int param)
{

  struct remote_screen* rscreen = (struct remote_screen*)_screen;

  if(rscreen->float_param_is_cached[param])
    return rscreen->cached_float_params[param];
  
  ALLOC_OUT_MESSAGE(get_paramf, message);
    
  message->base.opcode = REMREQ_GET_PARAMF;
  message->screen = SCREEN_HANDLE(_screen);
  message->param = param;
  
  if(analyse_waits)
    printf("WAIT: float param\n");
  QUEUE_AND_WAIT(message, get_paramf, reply);
  
  DBG("Get FP parameter %d: value is %f\n", param, reply->response);

  rscreen->cached_float_params[param] = reply->response;
  rscreen->float_param_is_cached[param] = 1;

  float response = reply->response;
  free_message(reply);
  return response;

}


static boolean 
remote_screen_is_format_supported(struct pipe_screen *_screen,
                                 enum pipe_format format,
                                 enum pipe_texture_target target,
                                 unsigned tex_usage, 
                                 unsigned geom_flags)
{

    ALLOC_OUT_MESSAGE(is_format_supported, message);
    
    message->base.opcode = REMREQ_IS_FORMAT_SUPPORTED;
    message->screen = SCREEN_HANDLE(_screen);
    message->format = format;
    message->target = target;
    message->tex_usage = tex_usage;
    message->geom_flags = geom_flags;
    
    if(analyse_waits)
      printf("WAIT: format_supported\n");
    QUEUE_AND_WAIT(message, is_format_supported, reply);
    
    if(reply->response)
	DBG("Format query: accepted\n");
    else
	DBG("Format query: denied\n");
    
    boolean response = reply->response;
    free_message(reply);
    return response;

}

#define minify(x) (((x >> 1) > 0) ? (x >> 1) : 1)

static struct pipe_texture *
remote_screen_texture_create(struct pipe_screen *_screen,
                            const struct pipe_texture *templat)
{
   struct remote_screen *tr_scr = remote_screen(_screen);

    ALLOC_OUT_MESSAGE(texture_create, message);
    
    uint32_t handle = get_fresh_texture_handle(_screen);
    
    DBG("Creating new texture, using handle %u\n", handle);
    
    message->base.opcode = REMREQ_TEXTURE_CREATE;
    message->templat = *templat;
    message->screen = tr_scr->remote_handle;
    message->handle = handle;
    
    enqueue_message(message);

    struct remote_texture* result = CALLOC_STRUCT(remote_texture);
	
    result->base = *templat; // Struct copy
    result->base.screen = _screen;
    result->base.refcount = 1;
    result->handle = handle;

    pf_get_block(result->base.format, &result->base.block);

    // This bit heavily based off softpipe's texture allocation stuff

    DBG("Texture create: %d levels, base dimension %dx%dx%d\n", result->base.last_level, result->base.width[0], result->base.height[0], result->base.depth[0]);

    unsigned int width = result->base.width[0];
    unsigned int height = result->base.height[0];
    unsigned int depth = result->base.depth[0];

    unsigned int buffer_size = 0;

    for(unsigned int i = 0; i <= result->base.last_level; i++) {

      result->base.width[i] = width;
      result->base.height[i] = height;
      result->base.depth[i] = depth;
      result->base.nblocksx[i] = pf_get_nblocksx(&result->base.block, width);
      result->base.nblocksy[i] = pf_get_nblocksy(&result->base.block, height);
      result->level_offset[i] = buffer_size;
      result->stride[i] = result->base.nblocksx[i] * result->base.block.size;

      int plane_size = result->base.nblocksx[i] * result->base.nblocksy[i] * result->base.block.size;

      buffer_size += (plane_size * ((result->base.target == PIPE_TEXTURE_CUBE) ? 6 : depth));

      DBG("Level %d consists of %dx%d blocks; block size %d; %d planes\n", i, result->base.nblocksx[i], result->base.nblocksy[i],  result->base.block.size, (result->base.target == PIPE_TEXTURE_CUBE ? 6 : depth));

      width = minify(width);
      height = minify(height);
      depth = minify(depth);

    }

    DBG("In total will allocate a buffer of size %d\n", buffer_size);

    result->backing_buffer = create_anon_buffer(buffer_size);

    if(!result->backing_buffer) {
      free(result);
      return 0;
    }
    else {
      return &(result->base);
    }

}


static struct pipe_texture *
remote_screen_texture_blanket(struct pipe_screen *_screen,
                             const struct pipe_texture *templat,
                             const unsigned *ppitch,
                             struct pipe_buffer *buffer)
{

  struct remote_screen *tr_scr = remote_screen(_screen);
   
  if(templat->target != PIPE_TEXTURE_2D ||
     templat->last_level != 0 ||
     templat->depth[0] != 1)
    return NULL;

  // All current Gallium drivers enforce this... and it seems as though
  // there is no way to express how you want extra mip-levels or planes
  // to be laid out in the buffer, so it's probably the only reasonable
  // thing to do with this interface.

    ALLOC_OUT_MESSAGE(texture_blanket, message);

    uint32_t handle = get_fresh_texture_handle(_screen);

    DBG("Blanket: created texture %u based on buffer %u\n", handle, BUFFER_HANDLE(buffer));
    
    message->base.opcode = REMREQ_TEXTURE_BLANKET;
    message->templat = *templat;
    message->screen = tr_scr->remote_handle;
    message->pitch = *ppitch;
    message->buffer = BUFFER_HANDLE(buffer);
    message->handle = handle;
    
    enqueue_message(message);
    
    struct remote_texture* result = CALLOC_STRUCT(remote_texture);
	
    result->base = *templat; // Struct copy
    result->base.screen = _screen;
    result->base.refcount = 1;
    result->handle = handle;

    pf_get_block(result->base.format, &result->base.block);

    // This bit heavily based off softpipe's texture allocation stuff

    pipe_buffer_reference(_screen, &result->backing_buffer, buffer);
    
    return &(result->base);
	
}


static void 
remote_screen_texture_release(struct pipe_screen *_screen,
                             struct pipe_texture **ptexture)
{

    if(*ptexture)
	DBG("Texture release: had refcount %d\n", (*ptexture)->refcount);
    else
	DBG("Texture release called against null pointer\n");

    if((*ptexture) && !(--((*ptexture)->refcount))) {
    
	DBG("Our local refcount reached zero; remote deref on texture %u\n", TEXTURE_HANDLE(*ptexture));
    
	ALLOC_OUT_MESSAGE(texture_release, message);

	message->base.opcode = REMREQ_TEXTURE_RELEASE;
	message->screen = SCREEN_HANDLE(_screen);
	message->texture = TEXTURE_HANDLE(*ptexture);
	
	enqueue_message(message);

	struct remote_texture* rtex = (struct remote_texture*)*ptexture;
	if(rtex->backing_buffer) {
	  pipe_buffer_reference(_screen, &rtex->backing_buffer, NULL);
	}
	else {
	  printf("!!! Freeing a texture with no backing buffer\n");
	}

	FREE(*ptexture);
	
    }
    
    *ptexture = NULL;

}

static int get_or_alloc_buffer_handle(struct remote_buffer* rbuf,
				      struct pipe_screen* screen,
				      uint32_t* handle) {

  if(rbuf->handle) {
    *handle = rbuf->handle;
    return 0;
  }
  else {
    *handle = get_fresh_buffer_handle(screen);
    rbuf->handle = *handle;
    return 1;
  }

}

static struct pipe_surface *
remote_screen_get_tex_surface(struct pipe_screen *_screen,
                             struct pipe_texture *texture,
                             unsigned face, unsigned level,
                             unsigned zslice,
                             unsigned usage)
{

    DBG("Get texture surface for texture %u\n", TEXTURE_HANDLE(texture));
    DBG("Face %u, level %u, zslice %u, usage %x\n", face, level, zslice, usage);

    struct remote_texture* rtex = (struct remote_texture*)texture;

    uint32_t buffer_handle;
    int new_name = get_or_alloc_buffer_handle((struct remote_buffer*)rtex->backing_buffer, _screen, &buffer_handle);
    // Gives the buffer a name, if it hadn't one already

    if(new_name)
      DBG("Get tex surface: the buffer had no name\n");
    else
      DBG("Get tex surface: the buffer had a name already\n");

    uint32_t surf_handle = get_fresh_surface_handle(_screen);

    DBG("Using handle %u for new surface\n", surf_handle);
    DBG("Using handle %u for its related buffer\n", buffer_handle);
    
    ALLOC_OUT_MESSAGE(get_tex_surface, message);
    
    message->base.opcode = REMREQ_GET_TEX_SURFACE;
    message->screen = SCREEN_HANDLE(_screen);
    message->texture = TEXTURE_HANDLE(texture);
    message->face = face;
    message->level = level;
    message->zslice = zslice;
    message->usage = usage;
    message->handle = surf_handle;
    message->buffer_handle = new_name ? buffer_handle : 0;
    // 0 here signifies that we already have a name for that buffer
    // This is the *only* way for anonymous buffers to get a name

    enqueue_message(message);
    
    struct remote_surface* result = CALLOC_STRUCT(remote_surface);

    result->base.winsys = _screen->winsys;
    result->base.texture = texture;
    result->base.face = face;
    result->base.level = level;
    result->base.zslice = zslice;
    result->base.format = texture->format;
    result->base.clear_value = 0;
    result->base.width = texture->width[level];
    result->base.height = texture->height[level];
    result->base.block = texture->block;
    result->base.nblocksx = texture->nblocksx[level];
    result->base.nblocksy = texture->nblocksy[level];
    result->base.stride = rtex->stride[level];
    result->base.offset = rtex->level_offset[level];

    if(texture->target == PIPE_TEXTURE_CUBE || texture->target == PIPE_TEXTURE_3D) {
      result->base.offset += ((texture->target == PIPE_TEXTURE_CUBE) ? face : zslice) * result->base.nblocksy * result->base.stride;
    }

    result->base.usage = usage;

    // ...and one is used for communication between the various driver components without a callback :(
    // In the current mesa-anything arrangment of state-tracker/pipe/winsys, the winsys calls into the Mesa ST
    // to notify st_notify_swapbuffers_complete, which sets pipe status to UNDEFINED; it then gets set back to DEFINED
    // by the next call to any drawing function (softpipe,) or upon a clear or get_tex_surface (i915).
    
    // For now I will always consider my surfaces to be defined, and since my winsys doesn't call notify_swapbuffers_complete,
    // this won't be interfered with. Its only use seems to be deciding how to clear a surface, so this could easily be
    // faked out at the remote side.

    // At the remote, then, the winsys might well call notify_complete, but I don't do anything about it, meaning buffers will be
    // DEFINED the first time the pipe chooses and stay that way. I might need to get the DEFINED/UNDEFINED cycle going on the remote,
    // set UNDEFINED here so I get Clear() calls and not clear-with-quad, and then finally recreate the "with-quad?" logic in the remote
    // state tracker depending on DEFINED'ness.

    result->base.status = PIPE_SURFACE_STATUS_DEFINED;

    // Not sure whether we or the remote are expected to fill this in: better safe than sorry.
    result->base.refcount = 1;

    result->handle = surf_handle;

    pipe_buffer_reference(_screen, &result->base.buffer, rtex->backing_buffer);
    
    return (&(result->base));
    
}


static void 
remote_screen_tex_surface_release(struct pipe_screen *_screen,
                                 struct pipe_surface **psurface)
{

    if(*psurface)
	DBG("Texture surface release on %u: old refcount %d\n", SURFACE_HANDLE(*psurface), (*psurface)->refcount);
    else
	DBG("Texture surface release passed a null pointer\n");

    if(*psurface && !(--((*psurface)->refcount))) {

        if((*psurface)->buffer) {
	    pipe_buffer_reference(_screen, &((*psurface)->buffer), NULL);
	}
	else {
	    printf("!!! tex_surface_release with no buffer\n");
	}
	
	ALLOC_OUT_MESSAGE(tex_surface_release, message);
	
	message->base.opcode = REMREQ_TEX_SURFACE_RELEASE;
	message->screen = SCREEN_HANDLE(_screen);
	message->surface = SURFACE_HANDLE(*psurface);
	
	enqueue_message(message);
	
	FREE(*psurface);
	
    }
    
    *psurface = NULL;
}


static void*
remote_screen_surface_map(struct pipe_screen *_screen,
                         struct pipe_surface *surface,
                         unsigned flags)
{

    DBG("Map surface %u (flags: %x)\n", SURFACE_HANDLE(surface), flags);
    DBG("(-->mapping buffer %u, with offset %u)\n", BUFFER_HANDLE(surface->buffer), surface->offset);

    char* map = (char*)pipe_buffer_map(_screen, surface->buffer, flags);

    return (void*)(map + surface->offset);
	
}


static void 
remote_screen_surface_unmap(struct pipe_screen *_screen,
                           struct pipe_surface *surface)
{

  DBG("Unmap surface %u\n", SURFACE_HANDLE(surface));

  pipe_buffer_unmap(_screen, surface->buffer);

}


static void
remote_screen_destroy(struct pipe_screen *_screen)
{
   struct remote_screen *tr_scr = remote_screen(_screen);

   DBG("Destroying screen %u: closing connection\n", SCREEN_HANDLE(_screen));

   close(tr_scr->socketfd);

   FREE(tr_scr);
}

struct pipe_screen *
remote_screen_create(struct pipe_winsys* winsys)
{
   
   struct remote_screen* tr_scr = CALLOC_STRUCT(remote_screen);

   memset(tr_scr, 0, sizeof(struct remote_screen));

   DBG("Creating new screen\n");
   
    if(!tr_scr) {
	printf("Alloc failed in remote_screen_create!\n");
	return NULL;
    }
  
   tr_scr->base.winsys = winsys;
   tr_scr->base.destroy = remote_screen_destroy;
   tr_scr->base.get_name = remote_screen_get_name;
   tr_scr->base.get_vendor = remote_screen_get_vendor;
   tr_scr->base.get_param = remote_screen_get_param;
   tr_scr->base.get_paramf = remote_screen_get_paramf;
   tr_scr->base.is_format_supported = remote_screen_is_format_supported;
   tr_scr->base.texture_create = remote_screen_texture_create;
   tr_scr->base.texture_blanket = remote_screen_texture_blanket;
   tr_scr->base.texture_release = remote_screen_texture_release;
   tr_scr->base.get_tex_surface = remote_screen_get_tex_surface;
   tr_scr->base.tex_surface_release = remote_screen_tex_surface_release;
   tr_scr->base.surface_map = remote_screen_surface_map;
   tr_scr->base.surface_unmap = remote_screen_surface_unmap;
   
   tr_scr->last_query_handle = 1;
   tr_scr->last_blend_handle = 1;
   tr_scr->last_dsa_handle = 1;
   tr_scr->last_rast_handle = 1;
   tr_scr->last_sampler_handle = 1;
   tr_scr->last_fs_handle = 1;
   tr_scr->last_vs_handle = 1;
   tr_scr->last_texture_handle = 1;
   tr_scr->last_surface_handle = 1;
   tr_scr->last_buffer_handle = 1;
   tr_scr->last_window_handle = 1;

   // Check if the user requests wait analysis
   
   analyse_waits = 0;

   char* analyse_waits_env = getenv("VG_ANALYSE_WAITS");
   if(analyse_waits_env) {
     if(analyse_waits_env[0] == '1') {
       printf("Enabled wait analysis\n");
       analyse_waits = 1;
     }
   }

   // Set up a Xen-IDC connection to describe graphics events to the host

   struct sockaddr_un sun;

   sun.sun_family = AF_UNIX;
   strcpy(sun.sun_path, "/var/run/xen3dd-socket");

   tr_scr->socketfd = socket(PF_UNIX, SOCK_STREAM, 0);
   DBG("Socket fd is %d\n", tr_scr->socketfd);

   if(tr_scr->socketfd == -1) {
     printf("socket() failed creating a new screen\n");
     exit(1);
   }

   if(connect(tr_scr->socketfd,
	      &sun,
	      sizeof(sun.sun_family) + strlen(sun.sun_path)) < 0) {
     printf("Couldn't connect to /var/run/xen3dd-socket\n");
     exit(1);
   }

   DBG("Successfully connected\n");

   // Now create two initally-empty ring buffers, one for RX and one for TX.

   struct {
     uint32_t* grants;
     void** map;
   } togrant[2] = {
     { .grants = tr_scr->rx_grants, .map = &(tr_scr->rx_buffer) },
     { .grants = tr_scr->tx_grants, .map = &(tr_scr->tx_buffer) }
   };

   DBG("tr_scr->rx_grants is %p, tr_scr->tx_grants is %p\n", tr_scr->rx_grants, tr_scr->tx_grants);

   for(int k = 0; k < 4; k++) {

     DBG("RX grant %d is currently %u\n", k, tr_scr->rx_grants[k]);
     DBG("TX grant %d is currently %u\n", k, tr_scr->tx_grants[k]);

   }

   for(int i = 0; i < 2; i++) {
     int fd = open("/dev/gntmem", O_RDWR);
     int ret;

     DBG("Opened /dev/gntmem; got fd %d\n", fd);
     
     if(fd == -1) {
       printf("Failed to open /dev/gntmem\n");
       exit(1);
     }
     
     ret = ioctl(fd, IOCTL_GNTMEM_SET_DOMAIN, 0);

     DBG("set-domain 0 returned %d\n", ret);

     if(ret == -1) {
       printf("Failed to set gntmem's target domain\n");
       exit(1);
     }

     ret = ioctl(fd, IOCTL_GNTMEM_SET_SIZE, 4);

     DBG("set-size 4 returned %d\n", ret);

     if(ret == -1) {
       printf("Failed to create a shareable section of 4 pages\n");
       exit(1);
     }

     DBG("Going to write grants starting at address %p\n", togrant[i].grants);
    
     ret = ioctl(fd, IOCTL_GNTMEM_GET_GRANTS, togrant[i].grants);

     DBG("get-grants returned %d\n", ret);
     DBG("After get-grants, have address %p\n", togrant[i].grants);

     for(int j = 0; j < 4; j++) {

       DBG("Grant #%d is %u\n", j, (togrant[i].grants)[j]);

     }
     
     if(ret == -1) {
       printf("Failed to get grants for shared section\n");
       exit(1);
     }

     *(togrant[i].map) = (void*)-1;
     *(togrant[i].map) = mmap(0, 4096 * 4, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

     DBG("Mapped as a shared section: got address %p\n", *(togrant[i].map));
     
     if(*(togrant[i].map) == (void*)-1) {
       printf("Couldn't mmap our shared section\n");
       exit(1);
     }
     
     close(fd);
     // Will remain in existence until an munmap

     // Now init the ring-buffer before we announce to the daemon
     uint32_t* initdata = (uint32_t*)*(togrant[i].map);
     DBG("Configuring initdata at %p\n", initdata);
     uint32_t buffersize = (4096 * 4) - (sizeof(uint32_t) * 3);
     initdata[0] = buffersize;
     // total buffer size                                                          
     initdata[1] = 0;
     initdata[2] = 0;
     // Next byte to read and next-to-write respectively.  
     // Equality signals the buffer is empty, write = read - 1 signals it is full
   }

   DBG("Successfully created ring buffers\n");
   DBG("On exit have rx at %p, tx at %p\n", tr_scr->rx_buffer, tr_scr->tx_buffer);

   // Okay, both ring buffers exist. Now send the grants to the daemon.
   struct {
     uint32_t rx_grants[4];
     uint32_t tx_grants[4];
     char is_x;
   } tosend;

   for(int i = 0; i < 4; i++) {
     tosend.rx_grants[i] = tr_scr->rx_grants[i];
     tosend.tx_grants[i] = tr_scr->tx_grants[i];
   }

   tosend.is_x = 0;

   int bytessent = 0;
   
   while(bytessent < sizeof(tosend)) {

     DBG("Going around; bytessent = %d, about to try to to send %lu on fd %d\n", bytessent, sizeof(tosend) - bytessent, tr_scr->socketfd);

     int sent = send(tr_scr->socketfd, ((char*)&tosend) + bytessent, sizeof(tosend) - bytessent, 0);

     DBG("Just sent %d bytes\n", sent);

     if(sent <= 0) {
       printf("Failed to send init message to xen3dd: return %d with errno %d\n", sent, errno);
       exit(1);
     }
     else {
       bytessent += sent;
     }

   }

   DBG("Successfully relayed that to the compositor\n");
   
   return &(tr_scr->base);
   
}

int complete_screen_creation(struct remote_screen* screen) {
   
    ALLOC_OUT_MESSAGE(create_screen, message);
    
    message->base.opcode = REMREQ_CREATE_SCREEN;
    
    DBG("WAIT: Screen creation\n");
    QUEUE_AND_WAIT(message, create_screen, reply);
    
    uint32_t handle = reply->handle;
    
    free_message(reply);
        
    if(!handle)
	return 0;
    else {
      DBG("Got handle %u\n", handle);
	screen->remote_handle = handle;
	return 1;
    }

}


struct remote_screen *
remote_screen(struct pipe_screen *screen)
{
   assert(screen);
   assert(screen->destroy == remote_screen_destroy);
   return (struct remote_screen *)screen;
}

// Handle generation functions. Currently these just assign sequential numbers.
// If a 32-bit space for e.g. buffers becomes a problem will need to expand that.

uint32_t get_fresh_query_handle(struct pipe_screen* screen) {

    struct remote_screen* remscr = (struct remote_screen*)screen;
    return remscr->last_query_handle++;

}

uint32_t get_fresh_blend_handle(struct pipe_screen* screen) {

    struct remote_screen* remscr = (struct remote_screen*)screen;
    return remscr->last_blend_handle++;

}

uint32_t get_fresh_sampler_handle(struct pipe_screen* screen) {

    struct remote_screen* remscr = (struct remote_screen*)screen;
    return remscr->last_sampler_handle++;

}

uint32_t get_fresh_rast_handle(struct pipe_screen* screen) {

    struct remote_screen* remscr = (struct remote_screen*)screen;
    return remscr->last_rast_handle++;

}

uint32_t get_fresh_dsa_handle(struct pipe_screen* screen) {

    struct remote_screen* remscr = (struct remote_screen*)screen;
    return remscr->last_dsa_handle++;

}

uint32_t get_fresh_vs_handle(struct pipe_screen* screen) {

    struct remote_screen* remscr = (struct remote_screen*)screen;
    return remscr->last_vs_handle++;

}

uint32_t get_fresh_fs_handle(struct pipe_screen* screen) {

    struct remote_screen* remscr = (struct remote_screen*)screen;
    return remscr->last_fs_handle++;

}

uint32_t get_fresh_texture_handle(struct pipe_screen* screen) {

    struct remote_screen* remscr = (struct remote_screen*)screen;
    return remscr->last_texture_handle++;

}

uint32_t get_fresh_surface_handle(struct pipe_screen* screen) {

    struct remote_screen* remscr = (struct remote_screen*)screen;
    return remscr->last_surface_handle++;

}

uint32_t get_fresh_buffer_handle(struct pipe_screen* screen) {

    struct remote_screen* remscr = (struct remote_screen*)screen;
    return remscr->last_buffer_handle++;

}

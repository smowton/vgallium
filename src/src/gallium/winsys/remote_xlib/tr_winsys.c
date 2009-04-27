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

#include "util/u_memory.h"
#include "util/u_hash_table.h"

#include "remote_state.h"
#include "tr_screen.h"
#include "remote_util.h"
#include "remote_messages.h"
#include "remote_comms.h"
#include "remote_debug.h"
#include "tr_winsys.h"
#include "tr_context.h"

extern int analyse_waits;

struct remote_winsys* singleton_winsys = 0;
struct remote_screen* singleton_screen = 0;

struct pipe_context *
xmesa_create_pipe_context(XMesaContext xmesa, uint pixelformat)
{
   struct pipe_context *pipe;

   DBG("Creating a new xmesa_context\n");

   if(!singleton_winsys) {
     DBG("Found no winsys; creating\n");
     singleton_winsys = (struct remote_winsys*)remote_winsys_create();
   }

   if(!singleton_screen) {
     DBG("Found no screen; creating\n");
     singleton_screen = (struct remote_screen*)remote_screen_create((struct pipe_winsys*)singleton_winsys);
     if(!singleton_screen) {
        DBG("remote_screen_create returned NULL\n");
        exit(1);
     }
     if(complete_screen_creation(singleton_screen)) {
	singleton_winsys->screen = (struct remote_screen*)singleton_screen;
     }
     else {
        DBG("Complete_screen_creation failed\n");
        exit(1);
     }
   }

   pipe = remote_context_create((struct pipe_screen*)singleton_screen);

   if (pipe)
      pipe->priv = xmesa;

   return pipe;
}


static const char *
winsys_get_name(struct pipe_winsys *_winsys)
{

    return "Xen virtualised 3D (GLX)\n";

}


static void 
winsys_flush_frontbuffer(struct pipe_winsys *_winsys,
                               struct pipe_surface *surface,
                               void *context_private)
{

    // Ignore the context_private pointer, as I can't know where this came from#
    // or how to serialise it correctly.

  DBG("Flush frontbuffer\n");

    ALLOC_OUT_MESSAGE(flush_frontbuffer, message);
    
    message->base.opcode = REMREQ_FLUSH_FRONTBUFFER;
    message->screen = WINSYS_HANDLE(_winsys);
    message->surface = SURFACE_HANDLE(surface);
    
    enqueue_message(message);

}


// The following two methods stubbed, since all our surfaces are remote and these never get called
// from the Mesa state tracker. Of course, should some other ST do so, these need implementing.

static struct pipe_surface *
winsys_surface_alloc(struct pipe_winsys *_winsys)
{

    printf("SURFACE_ALLOC NOT IMPLEMENTED!\n");
    exit(1);
    
    return NULL;

}


static int
winsys_surface_alloc_storage(struct pipe_winsys *_winsys,
                                   struct pipe_surface *surface,
                                   unsigned width, unsigned height,
                                   enum pipe_format format,
                                   unsigned flags,
                                   unsigned tex_usage)
{

    printf("SURFACE_ALLOC_STORAGE NOT IMPLEMENTED!\n");
    exit(1);
    
    return 0;
}

// Stubbed as I don't have any surfaces which do not correspond to textures

static void
winsys_surface_release(struct pipe_winsys *_winsys, 
                             struct pipe_surface **psurface)
{

    printf("SURFACE_RELEASE NOT IMPLEMENTED!\n");
    exit(1);
    
}

struct pipe_buffer* create_anon_buffer(unsigned size) {

    struct remote_buffer* rembuf = CALLOC_STRUCT(remote_buffer);
    
    rembuf->base.alignment = 1;
    rembuf->base.usage = PIPE_BUFFER_USAGE_CPU_READ | PIPE_BUFFER_USAGE_CPU_WRITE;
    rembuf->base.size = size;
    rembuf->base.refcount = 1;
    rembuf->handle = 0;
    rembuf->nmaps = 0;
    rembuf->is_user = 0;
    rembuf->map = malloc(size);
    rembuf->remote_dirty = 0;
    rembuf->local_dirty = 0;
    
    return (struct pipe_buffer*)rembuf;

}
  
static struct pipe_buffer *
winsys_buffer_create(struct pipe_winsys *_winsys, 
                           unsigned alignment, 
                           unsigned usage,
                           unsigned size)
{

    struct remote_winsys* rwinsys = 
	(struct remote_winsys*)_winsys;

    uint32_t handle = get_fresh_buffer_handle((struct pipe_screen*)rwinsys->screen);

    DBG("New buffer %u (size %u)\n", handle, size);
    
    ALLOC_OUT_MESSAGE(buffer_create, message);
    
    message->base.opcode = REMREQ_BUFFER_CREATE;
    message->screen = WINSYS_HANDLE(_winsys);
    message->alignment = alignment;
    message->usage = usage;
    message->size = size;
    message->handle = handle;
    
    enqueue_message(message);
    
    struct remote_buffer* rembuf = CALLOC_STRUCT(remote_buffer);
    
    rembuf->base.alignment = alignment;
    rembuf->base.usage = usage;
    rembuf->base.size = size;
    rembuf->base.refcount = 1;
    rembuf->handle = handle;
    rembuf->nmaps = 0;
    rembuf->is_user = 0;
    rembuf->map = malloc(size);
    rembuf->remote_dirty = 0;
    rembuf->local_dirty = 0;
    
    return (struct pipe_buffer*)rembuf;

}


static struct pipe_buffer *
winsys_user_buffer_create(struct pipe_winsys *_winsys, 
                                void *data,
                                unsigned size)
{

    struct remote_winsys* rwinsys = 
	(struct remote_winsys*)_winsys;

    uint32_t handle = get_fresh_buffer_handle((struct pipe_screen*)rwinsys->screen);

    DBG("User buffer create: %u (size %u)\n", handle, size);
    
    struct remreq_user_buffer_create* header =
      (struct remreq_user_buffer_create*)
        allocate_message_memory(sizeof(struct remreq_user_buffer_create) + size);
        
    header->base.opcode = REMREQ_USER_BUFFER_CREATE;
    header->screen = WINSYS_HANDLE(_winsys);
    header->size = size;
    header->handle = handle;
    
    void* copydest = ((char*)header + sizeof(struct remreq_user_buffer_create));
    
    memcpy(copydest, data, size);
    
    enqueue_message(header);
    
    struct remote_buffer* rembuf = CALLOC_STRUCT(remote_buffer);
    
    rembuf->base.alignment = 1;
    rembuf->base.size = size;
    rembuf->base.refcount = 1;
    rembuf->handle = handle;
    rembuf->nmaps = 0;
    rembuf->is_user = 1;
    rembuf->map = data;
    rembuf->remote_dirty = 0;
    rembuf->local_dirty = 0;
    
    return (struct pipe_buffer*)rembuf;

}


void
winsys_user_buffer_update(struct pipe_winsys *_winsys, 
                                struct pipe_buffer *buffer)
{

    // For now, copy the whole damn thing again.
    // In the long run, shared memory is the answer.

  DBG("Update user buffer %u\n", BUFFER_HANDLE(buffer));
    
    struct remreq_user_buffer_update* header =
      (struct remreq_user_buffer_update*)
        malloc(sizeof(struct remreq_user_buffer_update) + buffer->size);
    
    header->base.opcode = REMREQ_USER_BUFFER_UPDATE;
    header->screen = WINSYS_HANDLE(_winsys);
    header->buffer = BUFFER_HANDLE(buffer);
    
    void* copydest = ((char*)header + sizeof(struct remreq_user_buffer_create));
    
    void* copysrc = ((struct remote_buffer*)buffer)->map;
    
    memcpy(copydest, copysrc, buffer->size);
    
    enqueue_message(header);

}


static void *
winsys_buffer_map(struct pipe_winsys *_winsys, 
                        struct pipe_buffer *buffer,
                        unsigned usage)
{

  struct remote_buffer* rembuf = (struct remote_buffer*)buffer;

  if(usage & (PIPE_BUFFER_USAGE_CPU_WRITE))
    DBG("Map buffer %u for writing\n", BUFFER_HANDLE(buffer));
  else
    DBG("Map buffer %u for reading\n", BUFFER_HANDLE(buffer));

  if(rembuf->remote_dirty)
    DBG("Buffer is out of date: must update\n");

  if(rembuf->remote_dirty) {

    rembuf->remote_dirty = 0;
    // i.e., our local copy no longer lags the remote

    if(rembuf->nmaps > 1)
      printf("WARNING: Buffer was dirty, but already mapped locally.\n");

    ALLOC_OUT_MESSAGE(buffer_get_data, message);
    
    message->base.opcode = REMREQ_BUFFER_GET_DATA;
    message->winsys = WINSYS_HANDLE(_winsys);
    message->buffer = BUFFER_HANDLE(buffer);
    
    if(analyse_waits)
      printf("Might wait: buffer_get_data (getting %d bytes...)\n", (int)rembuf->base.size);
    QUEUE_AND_WAIT(message, buffer_get_data, reply);

    if(analyse_waits)
      printf("buffer_get_data completed\n");

    if(!reply->success) {
      DBG("Remote side reported failure\n");
      free_message(reply);
      return NULL;
    }
    
    memcpy(rembuf->map, ((char*)reply) + sizeof(struct remrep_buffer_get_data), rembuf->base.size);
    DBG("Copied in %u bytes\n", buffer->size);
    
    free_message(reply);

  }

  if(usage & (PIPE_BUFFER_USAGE_CPU_WRITE | PIPE_BUFFER_USAGE_GPU_WRITE))
    rembuf->local_dirty = 1;

  rembuf->nmaps++;
    
  return rembuf->map;

}


static void
winsys_buffer_unmap(struct pipe_winsys *_winsys, 
                          struct pipe_buffer *buffer)
{

    struct remote_buffer* rembuf = (struct remote_buffer*)buffer;

    DBG("Buffer unmap: buffer %u, currently mapped %u times\n", BUFFER_HANDLE(buffer), rembuf->nmaps);
    if(rembuf->local_dirty) {
      DBG("Remote is out of date: must update\n");
    }
    
    if((!(--(rembuf->nmaps))) && rembuf->local_dirty) {

      DBG("Local refcount hit zero: freeing map\n");

      if(analyse_waits)
	printf("Set data (might cause waits): %d bytes\n", (int)buffer->size);

      struct remreq_buffer_set_data* header =
	(struct remreq_buffer_set_data*)
        allocate_message_memory(sizeof(struct remreq_buffer_set_data)
				+ buffer->size);
        
      header->base.opcode = REMREQ_BUFFER_SET_DATA;
      header->winsys = WINSYS_HANDLE(_winsys);
      header->buffer = BUFFER_HANDLE(buffer);

      char* copydest = ((char*)header + sizeof(struct remreq_buffer_set_data));
      memcpy(copydest, rembuf->map, buffer->size);
    
      enqueue_message(header);

      if(analyse_waits)
	printf("Set data completed\n");

      rembuf->local_dirty = 0;
    
    }

}


static void
winsys_buffer_destroy(struct pipe_winsys *_winsys,
                            struct pipe_buffer *buffer)
{

  struct remote_buffer* rbuf = (struct remote_buffer*)buffer;

  if(rbuf->handle) {

    DBG("Destroy buffer %u\n", BUFFER_HANDLE(buffer));

    ALLOC_OUT_MESSAGE(buffer_destroy, message);
    
    message->base.opcode = REMREQ_BUFFER_DESTROY;
    message->screen = WINSYS_HANDLE(_winsys);
    message->buffer = BUFFER_HANDLE(buffer);

    enqueue_message(message);

  }
  else {
    DBG("Destroy anonymous buffer\n");
  }

  if(!rbuf->is_user) 
    free(rbuf->map);

  free(rbuf);

}

// Stubbed, as it doesn't appear the state tracker uses these

static void
winsys_fence_reference(struct pipe_winsys *_winsys,
                             struct pipe_fence_handle **pdst,
                             struct pipe_fence_handle *src)
{

  DBG("FENCE FUNCTIONS NOT IMPLEMENTED\n");

}


static int
winsys_fence_signalled(struct pipe_winsys *_winsys,
                             struct pipe_fence_handle *fence,
                             unsigned flag)
{

  DBG("FENCE FUNCTIONS NOT IMPLEMENTED\n");
  return 0;
 
}


static int
winsys_fence_finish(struct pipe_winsys *_winsys,
                          struct pipe_fence_handle *fence,
                          unsigned flag)
{

  DBG("FENCE FUNCTIONS NOT IMPLEMENTED\n");
  return 0;

}


static void
winsys_destroy(struct pipe_winsys *_winsys)
{
   struct remote_winsys *tr_ws = (struct remote_winsys*)_winsys;

   DBG("Destroy winsys for screen %u\n", WINSYS_HANDLE(_winsys));

   FREE(tr_ws);
}


struct pipe_winsys * remote_winsys_create()
{
   struct remote_winsys *tr_ws;

   DBG("Creating winsys\n");
   
   tr_ws = CALLOC_STRUCT(remote_winsys);
   if(!tr_ws)
      goto error1;

   tr_ws->base.destroy = winsys_destroy;
   tr_ws->base.get_name = winsys_get_name;
   tr_ws->base.flush_frontbuffer = winsys_flush_frontbuffer;
   tr_ws->base.surface_alloc = winsys_surface_alloc; 
   tr_ws->base.surface_alloc_storage = winsys_surface_alloc_storage;
   tr_ws->base.surface_release = winsys_surface_release;
   tr_ws->base.buffer_create = winsys_buffer_create;
   tr_ws->base.user_buffer_create = winsys_user_buffer_create; 
   tr_ws->base.buffer_map = winsys_buffer_map;
   tr_ws->base.buffer_unmap = winsys_buffer_unmap;
   tr_ws->base.buffer_destroy = winsys_buffer_destroy;
   tr_ws->base.fence_reference = winsys_fence_reference;
   tr_ws->base.fence_signalled = winsys_fence_signalled;
   tr_ws->base.fence_finish = winsys_fence_finish;
   
   return &tr_ws->base;
   
error1:
   return NULL;
   
}

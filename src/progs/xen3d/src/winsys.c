
#include "remote_messages.h"
#include "pipe/p_state.h"
#include "pipe/p_inlines.h"
#include "pipe/p_winsys.h"
#include "pipe/p_screen.h"
#include "pipe/p_context.h"
#include "util/u_memory.h"

#include "master_context.h"
#include "client.h"
#include "main.h"

#include <unistd.h>
#include <Hermes/Hermes.h>

#ifdef CS_DEBUG_WINSYS
#include <stdio.h>
#define DBG(format, args...) printf(format, ## args);
#else
#define DBG(format, args...)
#endif

#ifdef CS_DEBUG_MASTER
#include <stdio.h>
#define DBG2(format, args...) printf(format, ## args);
#else
#define DBG2(format, args...)
#endif

int dispatch_remreq_buffer_create(struct client_list_entry* client, struct remreq_buffer_create* message) {

  struct pipe_winsys* ws = client->screen->screen->winsys;

  struct pipe_buffer* new_buffer = ws->buffer_create(ws, message->alignment, message->usage, message->size);

  if(!new_buffer)
    return 0;

  struct client_buffer* buf_wrap = CALLOC_STRUCT(client_buffer);

  buf_wrap->base = new_buffer;
  buf_wrap->remote_texture_layout_may_differ = 0;

  MAP_ADD(uint32_t, struct client_buffer*, buffermap, &client->screen->buffers, message->handle, buf_wrap);

  return 1;

}

/* This is really just buffer_create and set_data in one. */
int dispatch_remreq_user_buffer_create(struct client_list_entry* client, struct remreq_user_buffer_create* message) {

  struct pipe_winsys* ws = client->screen->screen->winsys;

  void* data = ((char*)message) + sizeof(struct remreq_user_buffer_create);

  struct pipe_buffer* new_buffer = ws->buffer_create(ws, 1, PIPE_BUFFER_USAGE_CPU_READ | PIPE_BUFFER_USAGE_CPU_WRITE | PIPE_BUFFER_USAGE_GPU_READ | PIPE_BUFFER_USAGE_GPU_WRITE, message->size);

  if(!new_buffer)
    return 0;

  char* map = ws->buffer_map(ws, new_buffer, PIPE_BUFFER_USAGE_CPU_WRITE);
  
  memcpy(map, data, message->size);

  ws->buffer_unmap(ws, new_buffer);

  struct client_buffer* buf_wrap = CALLOC_STRUCT(client_buffer);

  buf_wrap->base = new_buffer;
  buf_wrap->remote_texture_layout_may_differ = 0;

  MAP_ADD(uint32_t, struct client_buffer*, buffermap, &client->screen->buffers, message->handle, buf_wrap);

  return 1;
		
}

int dispatch_remreq_user_buffer_update(struct client_list_entry* client, struct remreq_user_buffer_update* message) {

  // I think this was a function invented by the trace winsys!
  // If so, this is trouble, but will become non-trouble when user buffers are mapped
  // with true shared memory. In the meantime, could hack the Mesa state tracker.

  printf("user_buffer_update doesn't really exist!\n");

  return 0;

}

/* TEXTURE_SET_DATA AND GET_DATA:
   These two deal with the fact that local and remote texture layout is not synchronised, in the interests of not requiring a sync
   every time texture_create or get_tex_surface get called. They must map each surface in turn and serialize the buffer (i.e. texture),
   according to the following layout rules:

   1. No padding. All rows are packed; strides always equal pixel-size * width.
   2. If a texture has multiple levels of detail, these are concatenated together without padding.
   3. If a level of detail has multiple planes (i.e. it is a 3D texture or cube map,) these too are concatenated without padding.

   So in summary, for a cube map with two levels, we go (Face 0, level 0)(Face 1, level 0)(Face 2, level 0)...(Face 1, level 1) */

static int serialize_surface(struct client_list_entry* client, struct pipe_surface* surf, char** ptarget) {

  char* temp_map = client->screen->screen->surface_map(client->screen->screen,
						       surf, PIPE_BUFFER_USAGE_CPU_READ);

  if(!temp_map)
    return 0;

  int dest_stride = surf->nblocksx * surf->block.size;

  for(int row = 0; row < surf->nblocksy; row++, (*ptarget) += dest_stride, temp_map += surf->stride)
    memcpy((*ptarget), temp_map, dest_stride);

  client->screen->screen->surface_unmap(client->screen->screen, surf);

  return 1;

}

static int serialize_level_cube(struct client_list_entry* client, struct pipe_texture* tex, int level, char** ptarget) {

  int failed = 0;

  for(int face = 0; face < 6; face++) {

    struct pipe_surface* surf = client->screen->screen->get_tex_surface(client->screen->screen,
									tex, face, level, 0 /* zslice */,
									PIPE_BUFFER_USAGE_CPU_READ);

    if(!surf)
      return 0;

    if(!serialize_surface(client, surf, ptarget))
      failed = 1;

    client->screen->screen->tex_surface_release(client->screen->screen, &surf);

    if(failed)
      return 0;

  }

  return 1;

}

static int serialize_level_noncube(struct client_list_entry* client, struct pipe_texture* tex, int level, char** ptarget) {

  int failed = 0;

  for(int slice = 0; slice < tex->depth[level]; slice++) {

    struct pipe_surface* surf = client->screen->screen->get_tex_surface(client->screen->screen,
									tex, 0 /* face */, level, slice,
									PIPE_BUFFER_USAGE_CPU_READ);

    if(!surf)
      return 0;

    if(!serialize_surface(client, surf, ptarget))
      failed = 1;

    client->screen->screen->tex_surface_release(client->screen->screen, &surf);

    if(failed)
      return 0;

  }

  return 1;

}

static int serialize_level(struct client_list_entry* client, struct pipe_texture* tex, int level, char** ptarget) {

  if(tex->target == PIPE_TEXTURE_CUBE)
    return serialize_level_cube(client, tex, level, ptarget);
  else
    return serialize_level_noncube(client, tex, level, ptarget);

}

static int texture_get_data(struct client_list_entry* client, struct remreq_buffer_get_data* message, struct client_buffer* buffer) {

  struct client_texture* tex = buffer->texture;

  struct remrep_buffer_get_data* reply =
    allocate_message_memory(sizeof(struct remrep_buffer_get_data) + tex->client_size);

  reply->base.opcode = REMREP_BUFFER_GET_DATA;

  if(!reply) {
    printf("Out of memory passing out a surface\n");
    return 0;
  }

  char* target = ((char*)reply) + sizeof(struct remrep_buffer_get_data);

  int success = 1;

  for(int i = 0; success && (i <= tex->base->last_level); i++) {

    success = serialize_level(client, tex->base, i, &target);

  }

  reply->success = success;
  
  send_message(client, reply);

  return 1;

}

static int deserialize_surface(struct client_list_entry* client, struct pipe_surface* surf, char** ptarget) {


  char* temp_map = client->screen->screen->surface_map(client->screen->screen,
						       surf,
						       PIPE_BUFFER_USAGE_CPU_WRITE);

  if(!temp_map)
    return 0;

  int surface_size = surf->nblocksx * surf->nblocksy * surf->block.size;

  int src_stride = surf->nblocksx * surf->block.size;

  for(int row = 0; row < surf->nblocksy; row++, (*ptarget) += src_stride, temp_map += surf->stride)
    memcpy(temp_map, (*ptarget), src_stride);

  client->screen->screen->surface_unmap(client->screen->screen, surf);

  return 1;

}

static int deserialize_level_cube(struct client_list_entry* client, struct pipe_texture* tex, int level, char** ptarget) {

  int failed = 0;

  for(int face = 0; face < 6; face++) {

    struct pipe_surface* surf = client->screen->screen->get_tex_surface(client->screen->screen,
									tex, face, level, 0 /* zslice */,
									PIPE_BUFFER_USAGE_CPU_WRITE);

    if(!surf)
      return 0;

    if(!deserialize_surface(client, surf, ptarget))
      failed = 1;

    client->screen->screen->tex_surface_release(client->screen->screen, &surf);

    if(failed)
      return 0;

  }

  return 1;

}

static int deserialize_level_noncube(struct client_list_entry* client, struct pipe_texture* tex, int level, char** ptarget) {

  int failed = 0;

  for(int slice = 0; slice < tex->depth[level]; slice++) {

    struct pipe_surface* surf = client->screen->screen->get_tex_surface(client->screen->screen,
									tex, 0 /* face */, level, slice,
									PIPE_BUFFER_USAGE_CPU_WRITE);

    if(!surf)
      return 0;

    if(!deserialize_surface(client, surf, ptarget))
      failed = 1;

    client->screen->screen->tex_surface_release(client->screen->screen, &surf);

    if(failed)
      return 0;

  }

  return 1;

}

static int deserialize_level(struct client_list_entry* client, struct pipe_texture* tex, int level, char** ptarget) {

  if(tex->target == PIPE_TEXTURE_CUBE)
    return deserialize_level_cube(client, tex, level, ptarget);
  else
    return deserialize_level_noncube(client, tex, level, ptarget);

}

static int texture_set_data(struct client_list_entry* client, struct remreq_buffer_set_data* message, struct client_buffer* buffer) {

  struct pipe_texture* tex = buffer->texture->base;

  char* serialized_tex_data = ((char*)message) + sizeof(struct remreq_buffer_set_data);

  int success = 1;

  for(int i = 0; success && (i <= tex->last_level); i++) {

    success = deserialize_level(client, tex, i, &serialized_tex_data);

  }

  return success;

}

static int buffer_get_data(struct client_list_entry* client, struct remreq_buffer_get_data* message, struct client_buffer* target) {

  struct pipe_winsys* ws = client->screen->screen->winsys;

  int buffer_size = target->base->size;

  char* map = ws->buffer_map(ws, target->base, PIPE_BUFFER_USAGE_CPU_READ);

  struct remrep_buffer_get_data* reply =
    allocate_message_memory(sizeof(struct remrep_buffer_get_data) + buffer_size);

  reply->base.opcode = REMREP_BUFFER_GET_DATA;

  if(!map) {
    reply->success = 0;
  }
  else {
    reply->success = 1;

    char* copydest = ((char*)reply) + sizeof(struct remrep_buffer_get_data);
    memcpy(copydest, map, buffer_size);

    ws->buffer_unmap(ws, target->base);
  }
  
  send_message(client, reply);
  
  return 1;	
	
}

static int buffer_set_data(struct client_list_entry* client, struct remreq_buffer_set_data* message, struct client_buffer* target) {

  struct pipe_winsys* ws = client->screen->screen->winsys;

  char* map = ws->buffer_map(ws, target->base, PIPE_BUFFER_USAGE_CPU_WRITE);

  if(!map)
    return 0;

  char* copysrc = ((char*)message) + sizeof(struct remreq_buffer_set_data);

  memcpy(map, copysrc, target->base->size);

  ws->buffer_unmap(ws, target->base);
  
  return 1;	
	
}

int dispatch_remreq_buffer_get_data(struct client_list_entry* client, struct remreq_buffer_get_data* message) {

  struct client_buffer* target = MAP_DEREF(uint32_t, struct client_buffer*, buffermap, &client->screen->buffers, message->buffer);

  if(target->remote_texture_layout_may_differ)
    return texture_get_data(client, message, target);
  else
    return buffer_get_data(client, message, target);

}

int dispatch_remreq_buffer_set_data(struct client_list_entry* client, struct remreq_buffer_set_data* message) {

  struct client_buffer* target = MAP_DEREF(uint32_t, struct client_buffer*, buffermap, &client->screen->buffers, message->buffer);

  if(target->remote_texture_layout_may_differ)
    return texture_set_data(client, message, target);
  else
    return buffer_set_data(client, message, target);

}

int dispatch_remreq_buffer_destroy(struct client_list_entry* client, struct remreq_buffer_destroy* message) {

  struct pipe_winsys* ws = client->screen->screen->winsys;

  struct client_buffer* target = MAP_DEREF(uint32_t, struct client_buffer*, buffermap, &client->screen->buffers, message->buffer);

  /* This means that the remote is discarding a name for a buffer -- i.e. it has lost all references to it.

     Simply free the associated client_buffer and remove its entry from the buffer map. */

  winsys_buffer_reference(ws, &(target->base), NULL);

  free(target);

  MAP_DELETE(uint32_t, struct client_buffer*, buffermap, &client->screen->buffers, message->buffer);

  return 1;

}

int dispatch_remreq_swap_buffers(struct client_list_entry* client, struct remreq_swap_buffers* message) {

  struct client_texture* target = MAP_DEREF(uint32_t, struct client_texture*, texturemap, &client->screen->textures, message->texture);

  DBG2("Swap buffers on texture %u\n", message->texture);

  if(!target->frontbuffer) {
    // This texture hasn't been flipped before. Create a double-buffering
    // arrangement and add it to the global list of textures which should be
    // drawn by the master context.
    
    // Note that the X extension might have begun reporting locations and clipping rectangles
    // for this buffer before we flipped it, so it might be in the list already; in this case we
    // need to find that entry and register this texture against it.

    DBG2("No frontbuffer\n");

    struct pipe_texture template;

    memcpy(&template, target->base, sizeof(struct pipe_texture));

    template.format = client->global_state->window_pipe_format;

    pf_get_block(template.format, &template.block);

    target->frontbuffer = client->screen->screen->texture_create(client->screen->screen, &template);
    // A frontbuffer with the same format as the master window

    if(!target->frontbuffer) {
      printf("Failed to create a frontbuffer texture\n");
      return 0;
    }

    DBG2("Created a frontbuffer texture using %p as a template\n", target->base);

    uint32_t rmask, gmask, bmask, amask = 0;

    int backbuffer_pixel_size;

    switch(target->base->format) {

    case PIPE_FORMAT_A8R8G8B8_UNORM:
      amask = 0xFF000000;
      // Fall-through                                                                                                   
    case PIPE_FORMAT_X8R8G8B8_UNORM:
    case PIPE_FORMAT_R8G8B8_UNORM:
      if(target->base->format != PIPE_FORMAT_R8G8B8_UNORM)
        backbuffer_pixel_size = 32;
      else
        backbuffer_pixel_size = 24;
      rmask = 0x00FF0000;
      gmask = 0x0000FF00;
      bmask = 0x000000FF;
      break;
    case 2:
      backbuffer_pixel_size = 16;
      rmask = 0x0000F800;
      gmask = 0x000007E0;
      bmask = 0x0000001F;
      break;
    default:
      printf("Couldn't deal with a 3D app with an unusual colour depth\n");
      client->screen->screen->texture_release(client->screen->screen, &target->frontbuffer);
      target->frontbuffer = 0;
      return 0;
    }

    target->hermes_format = Hermes_FormatNew(backbuffer_pixel_size, rmask, gmask, bmask, amask, 0);
    // Format describing the backbuffer

    if(!target->hermes_format) {
      printf("Failed to create a format (%x, %x, %x, %x, pixel-size %d)\n",
	      rmask, gmask, bmask, amask, backbuffer_pixel_size);
      client->screen->screen->texture_release(client->screen->screen, &target->frontbuffer);
      target->frontbuffer = 0;
      return 0;
    }

    // Create a new texture using the existing surface's texture as a template

    struct drawable_surface_list** list = &client->domain->drawables;
    
    while(*list && (((*list)->screen != client->screen->remote_id)
	           || ((*list)->window != message->window)))
      list = &((*list)->next);

    if(!*list) {
	*list = CALLOC_STRUCT(drawable_surface_list);
	memset(*list, 0, sizeof(struct drawable_surface_list));
	DBG("Drawable list entry created in swapbuffers!\n");
	(*list)->window = message->window;
	(*list)->screen = client->screen->remote_id;
    }

    (*list)->texture = target;

  }

  /* This used all to deal in surfaces rather than textures, because pipe->set_framebuffer_state does so.
     However, shadowing at the surface level caused problems, because set_framebuffer_state uses pipe_surface_reference
     to set its internal record of what the current surface is. This, for some reason, calls tex_surface_release even
 if the surface's refcount has not reached zero, meaning that the surface is effectively unbound from its texture
     the first time it gets unreferenced, rather than when it dies.

     Since this would mean tracking the textures involved rather than just the surfaces in any case, I rewrote it to
     deal with swaps on a texture level.
  */

  /* This lot also used to try to be clever with buffer flipping, feeding the shadow-texture in as the real deal in calls to set_framebuffer_state.
     However, since the backbuffer texture might have many views taken out on it, it gets messy. I could attach to each texture a list of surfaces
     which refer to it, and shadow each of those. At the moment it seems like a lot of effort to implement flipping this way for perhaps minimal
     performance gain; however I might put that back in later. */

  /*  DBG2("Replacing frontbuffers where appropriate...\n");
  for(int i = 0; i < target->fbcontext->fbstate.num_cbufs; i++) {
    if(target->fbcontext->ccbufs[i] == target) {

      struct pipe_surface* old_surf = target->fbcontext->fbstate.cbufs[i];

      struct pipe_surface* new_surf = client->screen->screen->get_tex_surface(client->screen->screen, 
									      target->frontbuffer, 
									      old_surf->face, old_surf->level,
									      old_surf->zslice, old_surf->usage);

      DBG2("Found colour buffer %d was the backbuffer; flipping\n", i);
      pipe_surface_reference(&(target->fbcontext->fbstate.cbufs[i]), new_surf);

    }
    }*/
    
  struct pipe_context* ctx = get_master_context(client->global_state);

  struct pipe_surface* copySrc = 
    client->screen->screen->get_tex_surface(client->screen->screen, 
					    target->base, 
					    0, 0, 0, 
					    PIPE_BUFFER_USAGE_CPU_READ);
  struct pipe_surface* copyDest = 
    client->screen->screen->get_tex_surface(client->screen->screen, 
					    target->frontbuffer, 
					    0, 0, 0, 
					    PIPE_BUFFER_USAGE_CPU_WRITE);

  if(copySrc->format == copyDest->format) {
    ctx->surface_copy(ctx, 0, copyDest, 0, 0, copySrc, 0, 0, copySrc->width, copySrc->height);
  }
  else {


    void* srcMap = client->screen->screen->surface_map(client->screen->screen,
						     copySrc,
						     PIPE_BUFFER_USAGE_CPU_READ);

    void* destMap = client->screen->screen->surface_map(client->screen->screen,
						      copyDest,
						      PIPE_BUFFER_USAGE_CPU_WRITE);

    if(!Hermes_ConverterRequest(client->global_state->hermes_handle, target->hermes_format, client->global_state->hermes_dest_format)) {
      printf("ConverterRequest failed\n");
      return 0;
    }

    if(!Hermes_ConverterCopy(client->global_state->hermes_handle,
			     srcMap, 0, 0, copySrc->width, copySrc->height, copySrc->stride,
			     destMap, 0, 0, copySrc->width, copySrc->height, copyDest->stride)) {
      printf("Conversion failed\n");
      return 0;
    }


    client->screen->screen->surface_unmap(client->screen->screen, copySrc);
    client->screen->screen->surface_unmap(client->screen->screen, copyDest);

  }

  client->screen->screen->tex_surface_release(client->screen->screen, &copySrc);
  client->screen->screen->tex_surface_release(client->screen->screen, &copyDest);

  //ctx->set_framebuffer_state(ctx, &target->fbcontext->fbstate);

  //struct pipe_texture* temp = target->frontbuffer;
  //target->frontbuffer = target->base;
  //target->base = temp;

  return 1;
		
}

int dispatch_remreq_flush_frontbuffer(struct client_list_entry* client, struct remreq_flush_frontbuffer* message) {

  // Stub; don't understand how this gets used yet

  printf("!!! Flush frontbuffer not implemented!\n");

  return 0;

}



#include <Hermes/Hermes.h>

#include "remote_messages.h"
#include "client.h"
#include "main.h"

#include "rawgal.h"

#include "pipe/p_state.h"
#include "pipe/p_screen.h"
#include "pipe/p_inlines.h"
#include "util/u_memory.h"

#ifdef CS_DEBUG_SCREEN
#include <stdio.h>
#define DBG(format, args...) printf(format, ## args);
#else
#define DBG(format, args...)
#endif

int dispatch_remreq_create_context(struct client_list_entry* client, struct remreq_create_context* message) {

   EGLContext ctx = eglCreateContext(client->global_state->egl_display, 
				     client->global_state->egl_config, 
				     EGL_NO_CONTEXT, NULL );
   if (!ctx) {
      printf("Error: eglCreateContext failed\n");
      return 0;
   }
   
   struct client_context* new_ctx = CALLOC_STRUCT(client_context);

   memset((void*)new_ctx, 0, sizeof(struct client_context));

   new_ctx->egl_ctx = ctx;
   new_ctx->pipe_ctx = get_current_pipe_context();
   uint32_t context_handle = client->screen->ctx_handles++;
   
   MAP_ADD(uint32_t, struct client_context*, contextmap, &client->screen->contexts, context_handle, new_ctx);

   ALLOC_MESSAGE(reply, create_context);

   reply->base.opcode = REMREP_CREATE_CONTEXT;
   reply->handle = context_handle;

   send_message(client, reply);

   return 1;

}

int dispatch_remreq_get_param(struct client_list_entry* client, struct remreq_get_param* message) {

  int result = client->screen->screen->get_param(client->screen->screen, message->param);

  ALLOC_MESSAGE(reply, get_param);

  reply->base.opcode = REMREP_GET_PARAM;
  reply->response = result;

  send_message(client, reply);

  return 1;
	
}

int dispatch_remreq_get_paramf(struct client_list_entry* client, struct remreq_get_paramf* message) {

  float result = client->screen->screen->get_paramf(client->screen->screen, message->param);

  ALLOC_MESSAGE(reply, get_param);

  reply->base.opcode = REMREP_GET_PARAM;
  reply->response = result;

  send_message(client, reply);

  return 1;
		
}

int dispatch_remreq_is_format_supported(struct client_list_entry* client, struct remreq_is_format_supported* message) {

  boolean result = client->screen->screen->is_format_supported(client->screen->screen,
							       message->format,
							       message->target,
							       message->tex_usage,
							       message->geom_flags);

  ALLOC_MESSAGE(reply, is_format_supported);

  reply->base.opcode = REMREP_IS_FORMAT_SUPPORTED;
  reply->response = result;

  send_message(client, reply);

  return 1;
		
}

#define minify(x) (((x >> 1) > 0 ? (x >> 1) : 1))

static int calculate_client_size(struct pipe_texture* tex) {

  int running_total = 0;

  struct pipe_format_block block;
  pf_get_block(tex->format, &block);

  int width = tex->width[0];
  int height = tex->height[0];
  int depth = tex->depth[0];

  for(int i = 0; i <= tex->last_level; i++) {

    int plane_size = pf_get_nblocksx(&block, width) * pf_get_nblocksy(&block, height) * block.size;
    if(tex->target == PIPE_TEXTURE_2D)
      running_total += plane_size;
    else if(tex->target == PIPE_TEXTURE_3D)
      running_total += (plane_size * depth);
    else if(tex->target == PIPE_TEXTURE_CUBE)
      running_total += (plane_size * 6);

    width = minify(width);
    height = minify(height);
    depth = minify(depth);

  }

  return running_total;

}

/* Modified to avoid synchronisation on texture-create */

int dispatch_remreq_texture_create(struct client_list_entry* client, struct remreq_texture_create* message) {

  pf_get_block(message->templat.format, &message->templat.block);

  DBG("Creating texture: width %d, height %d, depth %d\n", message->templat.width[0], message->templat.height[0], message->templat.depth[0]);

  struct pipe_texture* new_texture = client->screen->screen->texture_create(client->screen->screen, &message->templat);
  // Might fail, but we don't particularly care -- notify the client, who must realise what's happened eventually, and until then quietly
  // skip commands which would otherwise fail. 

  ALLOC_MESSAGE(reply, texture_create);

  reply->base.opcode = REMREP_TEXTURE_CREATE;

  if(!new_texture)
    reply->success = 0;
  else
    reply->success = 1;
  /*    reply->result = *new_texture;*/

  struct client_texture* tex_wrap = CALLOC_STRUCT(client_texture);
  tex_wrap->base = new_texture;
  tex_wrap->frontbuffer = NULL;
  tex_wrap->fbcontext = NULL;
  tex_wrap->client_request = message->templat; /* Struct copy */
  tex_wrap->client_size = calculate_client_size(&message->templat);

  MAP_ADD(uint32_t, struct client_texture*, texturemap, &client->screen->textures, message->handle, tex_wrap);

  send_message(client, reply);

  return 1;
		
}

int dispatch_remreq_texture_blanket(struct client_list_entry* client, struct remreq_texture_blanket* message) {

  struct pipe_buffer* buffer_to_wrap = MAP_DEREF(uint32_t, struct client_buffer*, buffermap, &client->screen->buffers, message->buffer)->base;

  struct pipe_texture* new_texture = client->screen->screen->texture_blanket(client->screen->screen, &message->templat, &message->pitch, buffer_to_wrap);

  ALLOC_MESSAGE(reply, texture_blanket);

  reply->base.opcode = REMREP_TEXTURE_BLANKET;

  if(!new_texture) {
    reply->success = 0;
  }
  else {
    reply->success = 1;
    /*    reply->result = *new_texture; */
  }

  struct client_texture* tex_wrap = CALLOC_STRUCT(client_texture);
  tex_wrap->base = new_texture;
  tex_wrap->frontbuffer = NULL;
  tex_wrap->fbcontext = NULL;
  
  MAP_ADD(uint32_t, struct client_texture*, texturemap, &client->screen->textures, message->handle, tex_wrap);

  send_message(client, reply);

  return 1;
		
}

int dispatch_remreq_texture_release(struct client_list_entry* client, struct remreq_texture_release* message) {

  // Called when remote refcount has reached zero

  struct client_texture* to_release = MAP_DEREF(uint32_t, struct client_texture*, texturemap, &client->screen->textures, message->texture);

  if(to_release->frontbuffer) {

    struct drawable_surface_list** list = &client->domain->drawables;

    while(*list) {
      if((*list)->texture == to_release) {
	//struct drawable_surface_list* li_to_delete = *list;
	//*list = (*list)->next;
	//free(li_to_delete);
	(*list)->texture = 0;
	// Retain information about the window, perhaps pending a resize.
	// Clean up this list entry when we get an explicit window-destroyed notification.
	break;
      }
      list = &((*list)->next);
    }

    Hermes_FormatFree(to_release->hermes_format);

    client->screen->screen->texture_release(client->screen->screen, &to_release->frontbuffer);
  }

  // Dereference the texture once; the remote is done with this texture.
  // if(...) because creation might have failed but been masked to the remote.

  if(to_release->base)
    client->screen->screen->texture_release(client->screen->screen, &to_release->base);

  // Free the client_texture wrapper but not the underlying pipe_texture.

  free(to_release);

  MAP_DELETE(uint32_t, struct client_texture*, texturemap, &client->screen->textures, message->texture);

  return 1;
		
}

int dispatch_remreq_get_tex_surface(struct client_list_entry* client, struct remreq_get_tex_surface* message) {

  struct client_texture* ctarget = MAP_DEREF(uint32_t, struct client_texture*, texturemap, &client->screen->textures, message->texture);
  struct pipe_texture* target = ctarget->base;

  DBG("Get texture surface for texture %u (usage %u)\n", message->texture, message->usage);

  struct pipe_surface* result = NULL;

  // Beware here: "target" might be null, because texture creation might have failed. If so, the remote was notified at that time.
  // Let them know of this consequent failure too; it helps for debugging; in the meantime hide the failure by propagating the 'error value' to the surface.
  // In other words, make a client_surface associated with no actual surface.

  if(target)
    result = client->screen->screen->get_tex_surface(client->screen->screen,
						     target,
						     message->face,
						     message->level,
						     message->zslice,
						     message->usage);

  ALLOC_MESSAGE(reply, get_tex_surface);
  
  reply->base.opcode = REMREP_GET_TEX_SURFACE;

  if(!result) {
    DBG("Get texture surface failed\n");
    reply->success = 0;
  }
  else {
    DBG("Get texture surface succeeded\n");
    reply->success = 1;
  }
    
  struct client_surface* surf_wrap = CALLOC_STRUCT(client_surface);
  surf_wrap->base = result;
  surf_wrap->texture = ctarget;

  if(message->buffer_handle) {

    /* If the remote specifies a buffer_handle, they are giving a *name* to the buffer associated with the parent texture, and which
       was previously not addressable to them. Therefore I create a client_buffer, which refs the actual buffer and add it to the buffer map.
       If they do not specify a handle, the remote already has a name for that texture's associated buffer and no adding need be done. */

    struct client_buffer* buf_wrap = CALLOC_STRUCT(client_buffer);

    buf_wrap->remote_texture_layout_may_differ = 1;
    buf_wrap->texture = ctarget;

    /* The remote has decided on a layout for this buffer before it was 'exposed' here; we must serialise this buffer to
       the remote by repeated surface_map'ing, using the remote's agreed data ordering. By contrast, ordinary buffers are
       byte-for-byte identical. This includes texture_blanket'ed ordinary buffers -- there must be a single way to parse
       those bytes as image data, else different pipe drivers would place different interpretations on the same buffer */
  
    if(result)
      pipe_buffer_reference(client->screen->screen, &(buf_wrap->base), result->buffer);
    else
      buf_wrap->base = NULL;

    MAP_ADD(uint32_t, struct client_buffer*, buffermap, &client->screen->buffers, message->buffer_handle, buf_wrap);

  }

  printf("Adding surface handle %u\n", message->handle);

  MAP_ADD(uint32_t, struct client_surface*, surfacemap, &client->screen->surfaces, message->handle, surf_wrap);

  send_message(client, reply);

  return 1;
		
}

int dispatch_remreq_tex_surface_release(struct client_list_entry* client, struct remreq_tex_surface_release* message) {

  // Called when remote refcount has reached zero

  struct client_surface* target = MAP_DEREF(uint32_t, struct client_surface*, surfacemap, &client->screen->surfaces, message->surface);

  // Deref the surface once: the remote is done with this surface.

  if(target->base)
    client->screen->screen->tex_surface_release(client->screen->screen, &target->base);

  MAP_DELETE(uint32_t, struct client_surface*, surfacemap, &client->screen->surfaces, message->surface);

  // Free the client_surface wrapper but not the underlying pipe_surface.

  FREE(target);

  return 1;

}

int dispatch_remreq_create_screen(struct client_list_entry* client, struct remreq_create_screen* message) {

  client->screen = CALLOC_STRUCT(client_screen);

  memset(client->screen, 0, sizeof(struct client_screen));
  
  client->screen->screen = client->global_state->screen;
  client->screen->ctx_handles = 1;

  ALLOC_MESSAGE(reply, create_screen);

  reply->base.opcode = REMREP_CREATE_SCREEN;
  reply->handle = client->global_state->screen_handles++;
  client->screen->remote_id = reply->handle;

  send_message(client, reply);

  return 1;

}


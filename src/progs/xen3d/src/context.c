
#include "pipe/p_state.h"
#include "pipe/p_context.h"
#include "pipe/p_compiler.h"
#include "pipe/p_inlines.h"
#include "util/u_memory.h"
#include "tgsi/tgsi_parse.h"
#include "tgsi/tgsi_dump.h"

#include "rawgal.h"

#include "client.h"
#include "main.h"

#include "remote_messages.h"

#ifdef CS_DEBUG_CONTEXT
#include <stdio.h>
#define DBG(format, args...) printf(format, ## args)
#else
#define DBG(format, args...)
#endif

struct pipe_context* get_as_current(struct client_context* cctx, struct client_list_entry* client) {

  if(client->global_state->current_egl_ctx != cctx->egl_ctx) {

    /*if(!eglMakeCurrent(client->global_state->egl_display, 
		       client->global_state->window_egl_surface, 
		       client->global_state->window_egl_surface, 
		       cctx->egl_ctx)) {
	printf("eglMakeCurrent failed!\n");
	exit(1);
	}*/
    
    //struct pipe_context* ctx = get_current_pipe_context();

    // Reestablish the context's framebuffer state, which may have been altered by the make-current

    /*    if(cctx->fbstate_valid)
	  ctx->set_framebuffer_state(ctx, &cctx->fbstate);*/
    
  }

  client->global_state->current_egl_ctx = cctx->egl_ctx;

  return cctx->pipe_ctx;

}



int dispatch_remreq_set_edgeflags(struct client_list_entry* client, struct remreq_set_edgeflags* message) {

  struct pipe_context* ctx = get_as_current(MAP_DEREF(uint32_t, struct client_context*, contextmap,
				            &client->screen->contexts, 
					    message->pipe), client);

  if(message->isnull)
    ctx->set_edgeflags(ctx, NULL);
  else
    ctx->set_edgeflags(ctx, &message->flag);

  return 1;

}

int dispatch_remreq_draw_arrays(struct client_list_entry* client, struct remreq_draw_arrays* message) {

  struct pipe_context* ctx = get_as_current(MAP_DEREF(uint32_t, struct client_context*, contextmap,
				            &client->screen->contexts, 
					    message->pipe), client);

  boolean success = ctx->draw_arrays(ctx, message->mode, message->start, message->count);
  /*
  ALLOC_MESSAGE(reply, draw_arrays);

  reply->base.opcode = REMREP_DRAW_ARRAYS;
  reply->success = success;

  send_message(client, reply);
  */
  return 1;

}

int dispatch_remreq_draw_elements(struct client_list_entry* client, struct remreq_draw_elements* message) {

  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);

  struct pipe_buffer* buf = MAP_DEREF(uint32_t, struct client_buffer*, buffermap,
				      &client->screen->buffers,
				      message->buffer)->base;

  DBG("Draw elements: mode %u, start %u, count %u, indexBuffer handle %u, index size %u\n", message->mode, message->start, message->count, message->buffer, message->indexSize);

  boolean success = ctx->draw_elements(ctx, buf, message->indexSize, message->mode, message->start,  message->count);
  /*
  ALLOC_MESSAGE(reply, draw_elements);

  reply->base.opcode = REMREP_DRAW_ELEMENTS;
  reply->success = success;

  send_message(client, reply);
  */
  return 1;
		
}

int dispatch_remreq_draw_range_elements(struct client_list_entry* client, struct remreq_draw_range_elements* message) {

  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);

  struct pipe_buffer* buf = MAP_DEREF(uint32_t, struct client_buffer*, buffermap,
				      &client->screen->buffers,
				      message->buffer)->base;

  boolean success = ctx->draw_range_elements(ctx, buf, message->indexSize, message->minIndex, message->maxIndex, message->mode, message->start,  message->count);
  /*
  ALLOC_MESSAGE(reply, draw_range_elements);

  reply->base.opcode = REMREP_DRAW_RANGE_ELEMENTS;
  reply->success = success;
  
  send_message(client, reply);
  */
  return 1;
		
}

int dispatch_remreq_create_query(struct client_list_entry* client, struct remreq_create_query* message) {
  
  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);

  struct pipe_query* new_query = ctx->create_query(ctx, message->query_type);

  if(!new_query)
    return 0;

  struct client_query* query_wrap = CALLOC_STRUCT(client_query);
  if(!query_wrap)
    return 0;

  query_wrap->base = new_query;

  MAP_ADD(uint32_t, struct client_query*, querymap, &cctx->queries, message->handle, query_wrap);

  return 1;
		
}

int dispatch_remreq_destroy_query(struct client_list_entry* client, struct remreq_destroy_query* message) {

  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);

  struct client_query* query_wrap = MAP_DEREF(uint32_t, struct client_query*, querymap, &cctx->queries, message->query);
  
  struct pipe_query* query = query_wrap->base;

  ctx->destroy_query(ctx, query);

  MAP_DELETE(uint32_t, struct client_query*, querymap, &cctx->queries, message->query);
  FREE(query_wrap);

  return 1;

}

int dispatch_remreq_begin_query(struct client_list_entry* client, struct remreq_begin_query* message) {

  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);

  struct client_query* query_wrap = MAP_DEREF(uint32_t, struct client_query*,querymap, &cctx->queries, message->query);
  
  struct pipe_query* query = query_wrap->base;

  ctx->begin_query(ctx, query);

  return 1;
}

int dispatch_remreq_end_query(struct client_list_entry* client, struct remreq_end_query* message) {

  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);	

  struct client_query* query_wrap = MAP_DEREF(uint32_t, struct client_query*, querymap, &cctx->queries, message->query);
  
  struct pipe_query* query = query_wrap->base;

  ctx->end_query(ctx, query);

  return 1;
}

int dispatch_remreq_get_query_result(struct client_list_entry* client, struct remreq_get_query_result* message) {

  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);

  struct client_query* query_wrap = MAP_DEREF(uint32_t, struct client_query*, querymap, &cctx->queries, message->query);
  
  struct pipe_query* query = query_wrap->base;

  uint64 result;
  boolean done = ctx->get_query_result(ctx, query, message->wait, &result);

  ALLOC_MESSAGE(reply, get_query_result);

  reply->base.opcode = REMREP_GET_QUERY_RESULT;
  reply->done = done;
  reply->result = result;

  send_message(client, reply);

  return 1;
		
}

int dispatch_remreq_create_blend_state(struct client_list_entry* client, struct remreq_create_blend_state* message) {

  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);

  void* new_blend_state = ctx->create_blend_state(ctx, &message->state);
  
  if(!new_blend_state)
    return 0;

  struct client_blend* blend_wrap = CALLOC_STRUCT(client_blend);
  if(!blend_wrap)
    return 0;

  blend_wrap->base = new_blend_state;

  MAP_ADD(uint32_t, struct client_blend*, blendmap, &cctx->blends, message->handle, blend_wrap);

  return 1;

}

int dispatch_remreq_bind_blend_state(struct client_list_entry* client, struct remreq_bind_blend_state* message) {

  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);	

  struct client_blend* blend_wrap = MAP_DEREF(uint32_t, struct client_blend*, blendmap, &cctx->blends, message->blend_handle);
  
  ctx->bind_blend_state(ctx, blend_wrap ? blend_wrap->base : NULL);

  return 1;
	
}

int dispatch_remreq_delete_blend_state(struct client_list_entry* client, struct remreq_delete_blend_state* message) {

  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);	

  struct client_blend* blend_wrap = MAP_DEREF(uint32_t, struct client_blend*, blendmap, &cctx->blends, message->blend_handle);
  
  ctx->delete_blend_state(ctx, blend_wrap->base);

  MAP_DELETE(uint32_t, struct client_blend*, blendmap, &cctx->blends, message->blend_handle);
  FREE(blend_wrap);

  return 1;	
}

int dispatch_remreq_create_sampler_state(struct client_list_entry* client, struct remreq_create_sampler_state* message) {
  
  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);

  void* new_samp_state = ctx->create_sampler_state(ctx, &message->state);
  
  if(!new_samp_state)
    return 0;

  struct client_sampler* s_wrap = CALLOC_STRUCT(client_sampler);
  if(!s_wrap)
    return 0;

  s_wrap->base = new_samp_state;

  DBG("Created sampler %u\n", message->handle);

  MAP_ADD(uint32_t, struct client_sampler*, samplermap, &cctx->samplers, message->handle, s_wrap);

  return 1;
	 	
}

int dispatch_remreq_bind_sampler_state(struct client_list_entry* client, struct remreq_bind_sampler_state* message) {

  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);

  void** states = malloc(sizeof(void*) * message->nstates);
  uint32_t* handles = (uint32_t*)(((char*)message) + sizeof(struct remreq_bind_sampler_state));
  int i;

  for(i = 0; i < message->nstates; i++) {
    struct client_sampler* s_wrap = MAP_DEREF(uint32_t, struct client_sampler*, samplermap, &cctx->samplers, handles[i]);
    DBG("Set sampler %d: handle %u resolved to %p\n", i, handles[i], s_wrap);
    states[i] = s_wrap ? s_wrap->base : NULL;
  }
    
  ctx->bind_sampler_states(ctx, message->nstates, states);

  free(states);

  return 1;
		
}

int dispatch_remreq_delete_sampler_state(struct client_list_entry* client, struct remreq_delete_sampler_state* message) {

  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);	

  struct client_sampler* s_wrap = MAP_DEREF(uint32_t, struct client_sampler*, samplermap, &cctx->samplers, message->sampler_handle);
  
  ctx->delete_sampler_state(ctx, s_wrap->base);

  DBG("Deleted sampler %u\n", message->sampler_handle);

  MAP_DELETE(uint32_t, struct client_sampler*, samplermap, &cctx->samplers, message->sampler_handle);
  FREE(s_wrap);

  return 1;	
	
}

int dispatch_remreq_create_rast_state(struct client_list_entry* client, struct remreq_create_rast_state* message) {

  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);	

  void* new_rast_state = ctx->create_rasterizer_state(ctx, &message->state);

  if(!new_rast_state)
    return 0;

  struct client_rast* r_wrap = CALLOC_STRUCT(client_rast);
  if(!r_wrap)
    return 0;

  r_wrap->base = new_rast_state;

  MAP_ADD(uint32_t, struct client_rast*, rastmap, &cctx->rasts, message->handle, r_wrap);

  return 1;
	
}

int dispatch_remreq_bind_rast_state(struct client_list_entry* client, struct remreq_bind_rast_state* message) {

  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);	

  struct client_rast* r_wrap = MAP_DEREF(uint32_t, struct client_rast*, rastmap, &cctx->rasts, message->rast_handle);
  
  ctx->bind_rasterizer_state(ctx, r_wrap ? r_wrap->base : NULL);

  return 1;
	
}

int dispatch_remreq_delete_rast_state(struct client_list_entry* client, struct remreq_delete_rast_state* message) {

  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);	

  struct client_rast* r_wrap = MAP_DEREF(uint32_t, struct client_rast*, rastmap, &cctx->rasts, message->rast_handle);
  
  ctx->delete_rasterizer_state(ctx, r_wrap->base);

  MAP_DELETE(uint32_t, struct client_rast*, rastmap, &cctx->rasts, message->rast_handle);
  FREE(r_wrap);

  return 1;
	
}

int dispatch_remreq_create_dsa_state(struct client_list_entry* client, struct remreq_create_dsa_state* message) {

  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);

  void* new_dsa_state = ctx->create_depth_stencil_alpha_state(ctx, &message->state);
  
  if(!new_dsa_state)
    return 0;

  struct client_dsa* r_wrap = CALLOC_STRUCT(client_dsa);
  if(!r_wrap)
    return 0;

  r_wrap->base = new_dsa_state;

  MAP_ADD(uint32_t, struct client_dsa*, dsamap, &cctx->dsas, message->handle, r_wrap);

  return 1;	
}

int dispatch_remreq_bind_dsa_state(struct client_list_entry* client, struct remreq_bind_dsa_state* message) {

  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);	

  struct client_dsa* r_wrap = MAP_DEREF(uint32_t, struct client_dsa*, dsamap, &cctx->dsas, message->dsa_handle);
  
  ctx->bind_depth_stencil_alpha_state(ctx, r_wrap ? r_wrap->base : NULL);

  return 1;
}

int dispatch_remreq_delete_dsa_state(struct client_list_entry* client, struct remreq_delete_dsa_state* message) {

  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);

  struct client_dsa* r_wrap = MAP_DEREF(uint32_t, struct client_dsa*, dsamap, &cctx->dsas, message->dsa_handle);
  
  ctx->delete_depth_stencil_alpha_state(ctx, r_wrap->base);

  MAP_DELETE(uint32_t, struct client_dsa*, dsamap, &cctx->dsas, message->dsa_handle);
  FREE(r_wrap);

  return 1;
}

int dispatch_remreq_create_fs_state(struct client_list_entry* client, struct remreq_create_fs_state* message) {
  
  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);

  struct pipe_shader_state new_fs_state;

  struct tgsi_token* new_token_stream = (struct tgsi_token*)(((char*)message) + sizeof(struct remreq_create_fs_state));

  int length = tgsi_num_tokens(new_token_stream);

  // XXX: By feeding an invalid token stream here the remote could trigger the TGSI parser
  // to cause a buffer overrun. Need to modify the TGSI module to cope with untrusted code -- basically just
  // with some sort of tgsi_num_tokens_n after the manner of strncpy which won't overrun a given buffer and will return
  // an error if a valid EOS is not found in that space.

  new_fs_state.tokens = malloc(sizeof(struct tgsi_token) * length);

  if(!new_fs_state.tokens)
    return 0;

  memcpy(new_fs_state.tokens, new_token_stream, sizeof(struct tgsi_token) * length);


  /*
  char buf[1024];

  tgsi_dump_str(new_fs_state.tokens, 0, buf, 1024);
  
  printf("Fragment shader %u:\n%s", message->fs_handle, buf, 1024);
  */
  void* new_fs = ctx->create_fs_state(ctx, &new_fs_state);
  
  if(!new_fs)
    return 0;

  struct client_fs* fs_wrap = CALLOC_STRUCT(client_fs);
  if(!fs_wrap)
    return 0;

  fs_wrap->base = new_fs;
  fs_wrap->tokens = new_fs_state.tokens;

  MAP_ADD(uint32_t, struct client_fs*, fsmap, &cctx->fss, message->fs_handle, fs_wrap);
  
  return 1;		


	
}

int dispatch_remreq_bind_fs_state(struct client_list_entry* client, struct remreq_bind_fs_state* message) {
	
  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);

  struct client_fs* fs_wrap = MAP_DEREF(uint32_t, struct client_fs*, fsmap, &cctx->fss, message->fs_handle);
  
  ctx->bind_fs_state(ctx, fs_wrap ? fs_wrap->base : NULL);

  return 1;

}

int dispatch_remreq_delete_fs_state(struct client_list_entry* client, struct remreq_delete_fs_state* message) {

  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);	

  struct client_fs* fs_wrap = MAP_DEREF(uint32_t, struct client_fs*, fsmap, &cctx->fss, message->fs_handle);
  
  ctx->delete_fs_state(ctx, fs_wrap->base);

  MAP_DELETE(uint32_t, struct client_fs*, fsmap, &cctx->fss, message->fs_handle);

  if(fs_wrap->tokens)
    free(fs_wrap->tokens);

  FREE(fs_wrap);

  return 1;
}

int dispatch_remreq_create_vs_state(struct client_list_entry* client, struct remreq_create_vs_state* message) {

  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);

  struct pipe_shader_state new_vs_state;
  
  struct tgsi_token* new_token_stream = (struct tgsi_token*)(((char*)message) + sizeof(struct remreq_create_vs_state));

  int length = tgsi_num_tokens(new_token_stream);

  // XXX: By feeding an invalid token stream here the remote could trigger the TGSI parser
  // to cause a buffer overrun. Need to modify the TGSI module to cope with untrusted code -- basically just
  // with some sort of tgsi_num_tokens_n after the manner of strncpy which won't overrun a given buffer and will return
  // an error if a valid EOS is not found in that space.

  new_vs_state.tokens = malloc(sizeof(struct tgsi_token) * length);

  if(!new_vs_state.tokens)
    return 0;

  memcpy(new_vs_state.tokens, new_token_stream, sizeof(struct tgsi_token) * length);

  /*  char buf[1024];

  tgsi_dump_str(new_vs_state.tokens, 0, buf, 1024);
  
  printf("Vertex shader %u:\n%s", message->vs_handle, buf, 1024);*/

  void* new_vs = ctx->create_vs_state(ctx, &new_vs_state);
  
  if(!new_vs)
    return 0;

  struct client_vs* vs_wrap = CALLOC_STRUCT(client_vs);
  if(!vs_wrap)
    return 0;

  vs_wrap->base = new_vs;
  vs_wrap->tokens = new_vs_state.tokens;

  MAP_ADD(uint32_t, struct client_vs*, vsmap, &cctx->vss, message->vs_handle, vs_wrap);

  return 1;
}

int dispatch_remreq_bind_vs_state(struct client_list_entry* client, struct remreq_bind_vs_state* message) {

  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);	

  struct client_vs* vs_wrap = MAP_DEREF(uint32_t, struct client_vs*, vsmap, &cctx->vss, message->vs_handle);
  
  ctx->bind_vs_state(ctx, vs_wrap ? vs_wrap->base : NULL);

  return 1;
	
}

int dispatch_remreq_delete_vs_state(struct client_list_entry* client, struct remreq_delete_vs_state* message) {

  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);

  struct client_vs* vs_wrap = MAP_DEREF(uint32_t, struct client_vs*, vsmap, &cctx->vss, message->vs_handle);
  
  ctx->delete_vs_state(ctx, vs_wrap->base);

  MAP_DELETE(uint32_t, struct client_vs*, vsmap, &cctx->vss, message->vs_handle);

  if(vs_wrap->tokens)
    free(vs_wrap->tokens);

  FREE(vs_wrap);

  return 1;	
}

int dispatch_remreq_set_blend_color(struct client_list_entry* client, struct remreq_set_blend_color* message) {

  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);

  ctx->set_blend_color(ctx, &message->state);

  return 1;


}

int dispatch_remreq_set_clip_state(struct client_list_entry* client, struct remreq_set_clip_state* message) {

  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);		

  ctx->set_clip_state(ctx, &message->state);

  return 1;
		
}

int dispatch_remreq_set_polygon_stipple(struct client_list_entry* client, struct remreq_set_polygon_stipple* message) {

  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);				

  ctx->set_polygon_stipple(ctx, &message->state);

  return 1;

}

int dispatch_remreq_set_scissor_state(struct client_list_entry* client, struct remreq_set_scissor_state* message) {

  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);

  ctx->set_scissor_state(ctx, &message->state);

  return 1;
				
}

int dispatch_remreq_set_viewport_state(struct client_list_entry* client, struct remreq_set_viewport_state* message) {

  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);	

  ctx->set_viewport_state(ctx, &message->state);

  return 1;
			
}

int dispatch_remreq_set_constant_buffer(struct client_list_entry* client, struct remreq_set_constant_buffer* message) {

  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);

  struct pipe_constant_buffer buf;

  buf.buffer = (MAP_DEREF(uint32_t, struct client_buffer*, buffermap, &client->screen->buffers, message->buffer))->base;
  buf.size = message->buffer_size;

  DBG("Set constant buffer: buffer handle %u (size %u) for shader %u\n", message->buffer, buf.size, message->shader);
  /*
  char* mapped = (char*)pipe_buffer_map(ctx->screen, buf.buffer, PIPE_BUFFER_USAGE_CPU_READ);

  printf("On setting, constant buffer goes like this:\n");
    
  int i;
  for(i = 0; i < buf.size; i++) {
    printf("%4d ", (int)mapped[i]);
    if(i % 8 == 7)
      printf("\n");
  }
  printf("\n");
  
  pipe_buffer_unmap(ctx->screen, buf.buffer);
  */
  ctx->set_constant_buffer(ctx, message->shader, message->index, &buf);

  return 1;

}

int dispatch_remreq_set_framebuffer_state(struct client_list_entry* client, struct remreq_set_framebuffer_state* message) {

  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
				            &client->screen->contexts, 
					    message->pipe);

  struct pipe_context* ctx = get_as_current(cctx, client);

  DBG("Set framebuffer state: message specifies %u colour buffers\n", message->fbnum_cbufs);

  struct pipe_framebuffer_state newstate;

  //  if(cctx->fbstate_valid) {
  //    DBG("Found context already had associated buffers; disassociating.\n");
  //    for(int i = 0; i < cctx->fbstate.num_cbufs; i++) {
  //      cctx->ccbufs[i]->fbcontext = 0;
  //    }
  //  }

  //  cctx->fbstate_valid = 1;

  //  cctx->fbstate.width = message->fbwidth;
  //  cctx->fbstate.height = message->fbheight;
  //  cctx->fbstate.num_cbufs = message->fbnum_cbufs;

  newstate.width = message->fbwidth;
  newstate.height = message->fbheight;
  newstate.num_cbufs = message->fbnum_cbufs;

  if(message->fbzsbuf) {
    DBG("Z/stencil buffer will be surface %u\n", message->fbzsbuf);
    newstate.zsbuf = MAP_DEREF(uint32_t, struct client_surface*, surfacemap, &client->screen->surfaces, message->fbzsbuf)->base;    
  }
  else {
    DBG("No Z/stencil buffer is specified\n");
    newstate.zsbuf = NULL;
  }

  for(int i = 0; i < message->fbnum_cbufs; i++) {
    DBG("Setting colour-buffer %d to surface %u\n", i, message->fbcbufs[i]);

    struct client_surface* col_surf = MAP_DEREF(uint32_t, struct client_surface*, surfacemap, &client->screen->surfaces, message->fbcbufs[i]);

    //cctx->ccbufs[i] = col_surf->texture;

    // Mark this surface as currently in use by this context
    //cctx->ccbufs[i]->fbcontext = cctx;

    newstate.cbufs[i] = col_surf->base;

  }

  for(int i = message->fbnum_cbufs; i < PIPE_MAX_COLOR_BUFS; i++) {
    newstate.cbufs[i] = 0;
  }

  ctx->set_framebuffer_state(ctx, &newstate);

  return 1;
		
}

int dispatch_remreq_set_sampler_textures(struct client_list_entry* client, struct remreq_set_sampler_textures* message) {

  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);

  struct pipe_texture** textures = malloc(sizeof(struct pipe_texture*) * message->num_textures);
  int i;

  uint32_t* tex_handles = (uint32_t*)(((char*)message) + sizeof(struct remreq_set_sampler_textures));

  for(i = 0; i < message->num_textures; i++)
    textures[i] = (MAP_DEREF(uint32_t, struct client_texture*, texturemap, &client->screen->textures, tex_handles[i]))->base;

  ctx->set_sampler_textures(ctx, message->num_textures, textures);

  free(textures);

  return 1;

}

int dispatch_remreq_set_vertex_buffers(struct client_list_entry* client, struct remreq_set_vertex_buffers* message) {

  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);	

  struct pipe_vertex_buffer* buffers = malloc(sizeof(struct pipe_vertex_buffer) * message->num_buffers);
  int i;

  DBG("Setting %u buffers\n", message->num_buffers);

  struct opaque_pipe_vertex_element* o_bufs =
    (struct opaque_pipe_vertex_element*)(((char*)message) + sizeof(struct remreq_set_vertex_buffers));

  for(i = 0; i < message->num_buffers; i++) {
    DBG("Slot %d assigned buffer %u\n", i, o_bufs[i].buffer);
    buffers[i].pitch = o_bufs[i].pitch;
    buffers[i].max_index = o_bufs[i].max_index;
    buffers[i].buffer_offset = o_bufs[i].buffer_offset;
    buffers[i].buffer = o_bufs[i].buffer ? 
      (MAP_DEREF(uint32_t, struct client_buffer*, buffermap, &client->screen->buffers, o_bufs[i].buffer))->base 
      : NULL;
  }

  ctx->set_vertex_buffers(ctx, message->num_buffers, buffers);

  free(buffers);

  return 1;		
}

int dispatch_remreq_set_vertex_elements(struct client_list_entry* client, struct remreq_set_vertex_elements* message) {

  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);	

  struct pipe_vertex_element* first_element =
    (struct pipe_vertex_element*)(((char*)message) + sizeof(struct remreq_set_vertex_elements));

  ctx->set_vertex_elements(ctx, message->num_elements, first_element);

  return 1;
			
}

int dispatch_remreq_surface_copy(struct client_list_entry* client, struct remreq_surface_copy* message) {

  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);

  struct pipe_surface* src = (MAP_DEREF(uint32_t, struct client_surface*, surfacemap, &client->screen->surfaces, message->src))->base;
  struct pipe_surface* dest = (MAP_DEREF(uint32_t, struct client_surface*, surfacemap, &client->screen->surfaces, message->dest))->base;

  ctx->surface_copy(ctx,
		    message->do_flip,
		    dest,
		    message->dx,
		    message->dy,
		    src,
		    message->sx,
		    message->sy,
		    message->w,
		    message->h);

  return 1;
			
}

int dispatch_remreq_surface_fill(struct client_list_entry* client, struct remreq_surface_fill* message) {

  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);

  struct pipe_surface* dest = (MAP_DEREF(uint32_t, struct client_surface*, surfacemap, &client->screen->surfaces, message->surface))->base;

  ctx->surface_fill(ctx,
		    dest,
		    message->x,
		    message->y,
		    message->w,
		    message->h,
		    message->value);

  return 1;

}

int dispatch_remreq_clear(struct client_list_entry* client, struct remreq_clear* message) {

  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);

  DBG("Clearing surface %u\n", message->surface);

  struct pipe_surface* dest = (MAP_DEREF(uint32_t, struct client_surface*, surfacemap, &client->screen->surfaces, message->surface))->base;

  ctx->clear(ctx, dest, message->clearValue);

  return 1;
				
}

int dispatch_remreq_flush(struct client_list_entry* client, struct remreq_flush* message) {

  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);

  struct pipe_fence_handle* handle;

  ctx->flush(ctx, message->flags, &handle);
  // Just forget the handle for now...

  return 1;

}

int dispatch_remreq_destroy_context(struct client_list_entry* client, struct remreq_destroy_context* message) {

  struct client_context* cctx = MAP_DEREF(uint32_t, struct client_context*, contextmap, 
					  &client->screen->contexts, message->pipe); 
  struct pipe_context* ctx = get_as_current(cctx, client);

  destroy_context(cctx);

  MAP_DELETE(uint32_t, struct client_context*, contextmap, &client->screen->contexts, message->pipe);

  FREE(cctx);

  return 1;
				
}

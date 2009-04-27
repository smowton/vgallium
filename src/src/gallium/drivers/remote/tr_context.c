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
#include "pipe/p_screen.h"
#include "tgsi/tgsi_parse.h"
#include "pipe/p_shader_tokens.h"
#include "pipe/p_inlines.h"

#include "tr_screen.h"
#include "tr_context.h"
#include "remote_comms.h"
#include "remote_state.h"
#include "remote_util.h"
#include "remote_messages.h"
#include "remote_debug.h"

static INLINE void
remote_context_set_edgeflags(struct pipe_context *_pipe,
                            const unsigned *bitfield)
{

  if(bitfield) {
    DBG("Set edge flags: %u\n", *bitfield);
  }
  else {
    DBG("Set edge flags: null\n");
  }

    ALLOC_OUT_MESSAGE(set_edgeflags, message);
    
    message->base.opcode = REMREQ_SET_EDGEFLAGS;
    message->pipe = PIPE_HANDLE(_pipe);
    if(bitfield) {
      message->isnull = 0;
      message->flag = *bitfield;
    }
    else {
      message->isnull = 1;
    }
    
    enqueue_message(message);

}

static INLINE void mark_surface_dirty(struct pipe_surface* surf) {

  struct remote_texture* rtex = (struct remote_texture*)surf->texture;

  if(!rtex) {
    printf("!!! Surface with no texture encountered\n");
  }
  else {
    struct remote_buffer* rbuf = (struct remote_buffer*)rtex->backing_buffer;
    rbuf->remote_dirty = 1;
  }

}

static INLINE void mark_framebuffer_dirty(struct pipe_context* pipe) {

  struct remote_context* rctx = (struct remote_context*)pipe;

  for(int i = 0; i < rctx->current_framebuffer_state.num_cbufs; i++)
    if(rctx->current_framebuffer_state.cbufs[i])
      mark_surface_dirty(rctx->current_framebuffer_state.cbufs[i]);

  if(rctx->current_framebuffer_state.zsbuf)
    mark_surface_dirty(rctx->current_framebuffer_state.zsbuf);

}

static INLINE boolean
remote_context_draw_arrays(struct pipe_context *_pipe,
                          unsigned mode, unsigned start, unsigned count)
{

    DBG("Draw arrays: mode %u, start %u, count %u\n", mode, start, count);

    mark_framebuffer_dirty(_pipe);

    ALLOC_OUT_MESSAGE(draw_arrays, message);
    
    message->base.opcode = REMREQ_DRAW_ARRAYS;
    message->pipe = PIPE_HANDLE(_pipe);
    message->mode = mode;
    message->start = start;
    message->count = count;

    enqueue_message(message);

    return true;
    /*
    QUEUE_AND_WAIT(message, draw_arrays, reply);
    PANIC_IF_NULL(reply);
    
    boolean success = reply->success;
    
    free_message(reply);

    return success;
    */

}


static INLINE boolean
remote_context_draw_elements(struct pipe_context *_pipe,
                          struct pipe_buffer *indexBuffer,
                          unsigned indexSize,
                          unsigned mode, unsigned start, unsigned count)

{

  DBG("Draw elements: using index buffer %u, size %u, mode %u, start %u, count %u\n", BUFFER_HANDLE(indexBuffer), indexSize, mode, start, count);

    mark_framebuffer_dirty(_pipe);

    ALLOC_OUT_MESSAGE(draw_elements, message);
    
    message->base.opcode = REMREQ_DRAW_ELEMENTS;
    message->pipe = PIPE_HANDLE(_pipe);
    message->buffer = BUFFER_HANDLE(indexBuffer);
    message->indexSize = indexSize;
    message->mode = mode;
    message->start = start;
    message->count = count;

    enqueue_message(message);

    return true;

    /*
    QUEUE_AND_WAIT(message, draw_elements, reply);
    PANIC_IF_NULL(reply);
    
    boolean success = reply->success;
    free_message(reply);
    
    return success;
    */

}


static INLINE boolean
remote_context_draw_range_elements(struct pipe_context *_pipe,
                                  struct pipe_buffer *indexBuffer,
                                  unsigned indexSize,
                                  unsigned minIndex,
                                  unsigned maxIndex,
                                  unsigned mode, 
                                  unsigned start, 
                                  unsigned count)
{

  DBG("Draw range elements: using index buffer %u, size %u, mode %u, start %u, count %u, limits %u/%u\n", BUFFER_HANDLE(indexBuffer), indexSize, mode, start, count, minIndex, maxIndex);

    mark_framebuffer_dirty(_pipe);

    ALLOC_OUT_MESSAGE(draw_range_elements, message);
    
    message->base.opcode = REMREQ_DRAW_RANGE_ELEMENTS;
    message->pipe = PIPE_HANDLE(_pipe);
    message->buffer = BUFFER_HANDLE(indexBuffer);
    message->indexSize = indexSize;
    message->minIndex = minIndex;
    message->maxIndex = maxIndex;
    message->mode = mode;
    message->start = start;
    message->count = count;

    enqueue_message(message);
    return true;
    
    /*
    QUEUE_AND_WAIT(message, draw_range_elements, reply);

    PANIC_IF_NULL(reply);
    
    boolean success = reply->success;
    free_message(reply);
    
    return success;
    */

}


static INLINE struct pipe_query *
remote_context_create_query(struct pipe_context *_pipe,
                           unsigned query_type)
{

// Despite appearances, pipe_query is actually an opaque struct.

    ALLOC_OUT_MESSAGE(create_query, message);
    
    message->base.opcode = REMREQ_CREATE_QUERY;
    message->pipe = PIPE_HANDLE(_pipe);
    message->query_type = query_type;
    
    uint32_t handle = get_fresh_query_handle(_pipe->screen);
    
    DBG("New query %u, type %x\n", handle, query_type);
    
    message->handle = handle;

    enqueue_message(message);
    
    struct remote_pipe_query* opaque = CALLOC_STRUCT(remote_pipe_query);
    
    PANIC_IF_NULL(opaque);
    
    opaque->handle = handle;
    
    return (struct pipe_query*)opaque;

}


static INLINE void
remote_context_destroy_query(struct pipe_context *_pipe,
                            struct pipe_query *query)
{

    DBG("Destroy query %u\n", QUERY_HANDLE(query));

    ALLOC_OUT_MESSAGE(destroy_query, message);
    
    message->base.opcode = REMREQ_DESTROY_QUERY;
    message->pipe = PIPE_HANDLE(_pipe);
    message->query = QUERY_HANDLE(query);
    
    enqueue_message(message);

}


static INLINE void
remote_context_begin_query(struct pipe_context *_pipe, 
                          struct pipe_query *query)
{

    DBG("Begin query %u\n", QUERY_HANDLE(query));

    ALLOC_OUT_MESSAGE(begin_query, message);
    
    message->base.opcode = REMREQ_BEGIN_QUERY;
    message->pipe = PIPE_HANDLE(_pipe);
    message->query = QUERY_HANDLE(query);
    
    enqueue_message(message);

}


static INLINE void
remote_context_end_query(struct pipe_context *_pipe, 
                        struct pipe_query *query)
{

    DBG("End query %u\n", QUERY_HANDLE(query));

    ALLOC_OUT_MESSAGE(end_query, message);
    
    message->base.opcode = REMREQ_END_QUERY;
    message->pipe = PIPE_HANDLE(_pipe);
    message->query = QUERY_HANDLE(query);
    
    enqueue_message(message);

}


static INLINE boolean
remote_context_get_query_result(struct pipe_context *_pipe, 
                               struct pipe_query *query,
                               boolean wait,
                               uint64 *presult)
{

    DBG("Get query result for query %u\n", QUERY_HANDLE(query));

    ALLOC_OUT_MESSAGE(get_query_result, message);
    
    message->base.opcode = REMREQ_GET_QUERY_RESULT;
    message->pipe = PIPE_HANDLE(_pipe);
    message->query = QUERY_HANDLE(query);
    message->wait = wait;
    
    QUEUE_AND_WAIT(message, get_query_result, reply);
    
    if(reply) {
	*presult = reply->result;
	DBG("Query result: %lu\n", reply->result);
	boolean done = reply->done;
	free_message(reply);
	return done;
    }
    else {
	printf("!!! Got a null reply to get_query_result\n");
	exit(1);
    }

}


static INLINE void *
remote_context_create_blend_state(struct pipe_context *_pipe,
                                 const struct pipe_blend_state *state)
{
    ALLOC_OUT_MESSAGE(create_blend_state, message);
    
    message->base.opcode = REMREQ_CREATE_BLEND_STATE;
    message->pipe = PIPE_HANDLE(_pipe);
    message->state = *state;
    // Struct copy

    uint32_t handle = get_fresh_blend_handle(_pipe->screen);
    message->handle = handle;
    
    DBG("Create blend state %u\n", handle);
    
    enqueue_message(message);
    
    struct remote_opaque_blend_state* remote_blend = CALLOC_STRUCT(remote_opaque_blend_state);
    
    PANIC_IF_NULL(remote_blend);
    remote_blend->handle = handle;
    return remote_blend;
}


static INLINE void
remote_context_bind_blend_state(struct pipe_context *_pipe, 
                               void *state)
{
    
    DBG("Bind blend state %u\n", BLEND_HANDLE(state));

    ALLOC_OUT_MESSAGE(bind_blend_state, message);
    
    message->base.opcode = REMREQ_BIND_BLEND_STATE;
    message->pipe = PIPE_HANDLE(_pipe);
    message->blend_handle = BLEND_HANDLE(state);
    
    enqueue_message(message);    
}


static INLINE void
remote_context_delete_blend_state(struct pipe_context *_pipe, 
                                 void *state)
{
    DBG("Delete blend state %u\n", BLEND_HANDLE(state));

    ALLOC_OUT_MESSAGE(delete_blend_state, message);
    
    message->base.opcode = REMREQ_DELETE_BLEND_STATE;
    message->pipe = PIPE_HANDLE(_pipe);
    message->blend_handle = BLEND_HANDLE(state);
    
    enqueue_message(message);    
}


static INLINE void *
remote_context_create_sampler_state(struct pipe_context *_pipe,
                                   const struct pipe_sampler_state *state)
{

    ALLOC_OUT_MESSAGE(create_sampler_state, message);
    
    message->base.opcode = REMREQ_CREATE_SAMPLER_STATE;
    message->pipe = PIPE_HANDLE(_pipe);
    message->state = *state;
    // Struct copy

    uint32_t handle = get_fresh_sampler_handle(_pipe->screen);
    message->handle = handle;
    
    DBG("Create new sampler state %u\n", handle);
    
    enqueue_message(message);
    
    struct remote_opaque_sampler_state* remote_sampler = CALLOC_STRUCT(remote_opaque_sampler_state);
    
    PANIC_IF_NULL(remote_sampler);
    remote_sampler->handle = handle;
    return remote_sampler;

}


static INLINE void
remote_context_bind_sampler_states(struct pipe_context *_pipe, 
                                  unsigned num_states, void **states)
{

    DBG("Bind sampler states: %u states\n", num_states);

    struct remreq_bind_sampler_state* header =
      allocate_message_memory(
          sizeof(struct remreq_bind_sampler_state)
	+ (num_states * sizeof(uint32_t)));
    
    header->base.opcode = REMREQ_BIND_SAMPLER_STATE;
    header->pipe = PIPE_HANDLE(_pipe);
    header->nstates = num_states;

    uint32_t* state_handles =
      (uint32_t*)((char*)header + sizeof(struct remreq_bind_sampler_state));    
    unsigned i;
    for(i = 0; i < num_states; i++) {
	state_handles[i] = SAMPLER_HANDLE(states[i]);	
    }

    enqueue_message(header);
}


static INLINE void
remote_context_delete_sampler_state(struct pipe_context *_pipe, 
                                   void *state)
{

    DBG("Delete sampler state %u\n", SAMPLER_HANDLE(state));

    ALLOC_OUT_MESSAGE(delete_sampler_state, message);
    
    message->base.opcode = REMREQ_DELETE_SAMPLER_STATE;
    message->pipe = PIPE_HANDLE(_pipe);
    message->sampler_handle = SAMPLER_HANDLE(state);
    
    enqueue_message(message);    
}


static INLINE void *
remote_context_create_rasterizer_state(struct pipe_context *_pipe,
                                      const struct pipe_rasterizer_state *state)
{

    ALLOC_OUT_MESSAGE(create_rast_state, message);
    
    message->base.opcode = REMREQ_CREATE_RAST_STATE;
    message->pipe = PIPE_HANDLE(_pipe);
    message->state = *state;
    // Struct copy

    uint32_t handle = get_fresh_rast_handle(_pipe->screen);
    message->handle = handle;
    
    DBG("Create new rasterizer state: handle %u\n", handle);
    
    enqueue_message(message);
    
    struct remote_opaque_rast_state* remote_rast = CALLOC_STRUCT(remote_opaque_rast_state);
    
    PANIC_IF_NULL(remote_rast);
    remote_rast->handle = handle;
    return remote_rast;
}


static INLINE void
remote_context_bind_rasterizer_state(struct pipe_context *_pipe, 
                                    void *state)
{

    ALLOC_OUT_MESSAGE(bind_rast_state, message);
    
    message->base.opcode = REMREQ_BIND_RAST_STATE;
    message->pipe = PIPE_HANDLE(_pipe);
    message->rast_handle = RAST_HANDLE(state);
    
    enqueue_message(message);    
}


static INLINE void
remote_context_delete_rasterizer_state(struct pipe_context *_pipe, 
                                      void *state)
{
    DBG("Delete rasterizer state %u\n", RAST_HANDLE(state));

    ALLOC_OUT_MESSAGE(delete_rast_state, message);
    
    message->base.opcode = REMREQ_DELETE_RAST_STATE;
    message->pipe = PIPE_HANDLE(_pipe);
    message->rast_handle = RAST_HANDLE(state);
    
    enqueue_message(message);    
}


static INLINE void *
remote_context_create_depth_stencil_alpha_state(struct pipe_context *_pipe,
                                               const struct pipe_depth_stencil_alpha_state *state)
{
    ALLOC_OUT_MESSAGE(create_dsa_state, message);
    
    message->base.opcode = REMREQ_CREATE_DSA_STATE;
    message->pipe = PIPE_HANDLE(_pipe);
    message->state = *state;
    // Struct copy

    uint32_t handle = get_fresh_dsa_handle(_pipe->screen);
    message->handle = handle;
    
    DBG("Create depth/stencil/alpha state: handle %u\n", handle);
    
    enqueue_message(message);
    
    struct remote_opaque_dsa_state* remote_dsa = CALLOC_STRUCT(remote_opaque_dsa_state);
    
    PANIC_IF_NULL(remote_dsa);
    remote_dsa->handle = handle;
    return remote_dsa;
}


static INLINE void
remote_context_bind_depth_stencil_alpha_state(struct pipe_context *_pipe, 
                                             void *state)
{

    DBG("Bind depth/stencil/alpha state %u\n", DSA_HANDLE(state));

    ALLOC_OUT_MESSAGE(bind_dsa_state, message);
    
    message->base.opcode = REMREQ_BIND_DSA_STATE;
    message->pipe = PIPE_HANDLE(_pipe);
    message->dsa_handle = DSA_HANDLE(state);
    
    enqueue_message(message);    
}


static INLINE void
remote_context_delete_depth_stencil_alpha_state(struct pipe_context *_pipe, 
                                               void *state)
{

    DBG("Delete depth/stencil/alpha state %u\n", DSA_HANDLE(state));

    ALLOC_OUT_MESSAGE(delete_dsa_state, message);
    
    message->base.opcode = REMREQ_DELETE_DSA_STATE;
    message->pipe = PIPE_HANDLE(_pipe);
    message->dsa_handle = DSA_HANDLE(state);
    
    enqueue_message(message);    
}


static INLINE void *
remote_context_create_fs_state(struct pipe_context *_pipe,
                              const struct pipe_shader_state *state)
{
    int tokens = tgsi_num_tokens(state->tokens);

    struct remreq_create_fs_state* header = 
	allocate_message_memory(
	    sizeof(struct remreq_create_fs_state) 
	  + (tokens * sizeof(struct tgsi_token)));
	  
    uint32_t handle = get_fresh_fs_handle(_pipe->screen);
    
    header->base.opcode = REMREQ_CREATE_FS_STATE;
    header->pipe = PIPE_HANDLE(_pipe);
    header->fs_handle = handle;

    char* copy_dest = ((char*)header) + sizeof(struct remreq_create_fs_state);
    memcpy(copy_dest, (char*)(state->tokens), sizeof(struct tgsi_token) * tokens);
    
    DBG("Create new FS state: handle %u\n", handle);
    
    enqueue_message(header);
    
    struct opaque_remote_fs* retval = CALLOC_STRUCT(opaque_remote_fs);
    
    PANIC_IF_NULL(retval);
    
    retval->handle = handle;
    
    return retval;
}


static INLINE void
remote_context_bind_fs_state(struct pipe_context *_pipe, 
                            void *state)
{
 
  if(state) {
    DBG("Bind fragment shader %u\n", FS_HANDLE(state));
  }
  else {
    DBG("Bind fragment shader: no shader\n");
  }

    ALLOC_OUT_MESSAGE(bind_fs_state, message);
    
    message->base.opcode = REMREQ_BIND_FS_STATE;
    message->pipe = PIPE_HANDLE(_pipe);
    message->fs_handle = FS_HANDLE(state);
    
    enqueue_message(message);    
}


static INLINE void
remote_context_delete_fs_state(struct pipe_context *_pipe, 
                              void *state)
{

    DBG("Delete fragment shader %u\n", FS_HANDLE(state));

    ALLOC_OUT_MESSAGE(delete_fs_state, message);
    
    message->base.opcode = REMREQ_DELETE_FS_STATE;
    message->pipe = PIPE_HANDLE(_pipe);
    message->fs_handle = FS_HANDLE(state);
    
    enqueue_message(message);
}


static INLINE void *
remote_context_create_vs_state(struct pipe_context *_pipe,
                              const struct pipe_shader_state *state)
{

    int tokens = tgsi_num_tokens(state->tokens);

    struct remreq_create_vs_state* header = 
	allocate_message_memory(
	    sizeof(struct remreq_create_vs_state) 
	  + (tokens * sizeof(struct tgsi_token)));
	  
    uint32_t handle = get_fresh_vs_handle(_pipe->screen);
    
    DBG("Create new VS state: handle %u\n", handle);
    
    header->base.opcode = REMREQ_CREATE_VS_STATE;
    header->pipe = PIPE_HANDLE(_pipe);
    header->vs_handle = handle;

    char* copy_dest = ((char*)header) + sizeof(struct remreq_create_vs_state);
    memcpy(copy_dest, (char*)(state->tokens), sizeof(struct tgsi_token) * tokens);
    
    enqueue_message(header);
    
    struct opaque_remote_vs* retval = CALLOC_STRUCT(opaque_remote_vs);
    
    PANIC_IF_NULL(retval);
    
    retval->handle = handle;
    
    return retval;

}


static INLINE void
remote_context_bind_vs_state(struct pipe_context *_pipe, 
                            void *state)
{

  if(state) {
    DBG("Bind vertex shader state %u\n", VS_HANDLE(state));
  }
  else {
    DBG("Bind vertex shader: no shader\n");
  }

    ALLOC_OUT_MESSAGE(bind_vs_state, message);
    
    message->base.opcode = REMREQ_BIND_VS_STATE;
    message->pipe = PIPE_HANDLE(_pipe);
    message->vs_handle = VS_HANDLE(state);
    
    enqueue_message(message);    

}


static INLINE void
remote_context_delete_vs_state(struct pipe_context *_pipe, 
                              void *state)
{

    DBG("Delete vertex shader state %u\n", VS_HANDLE(state));

    ALLOC_OUT_MESSAGE(delete_vs_state, message);
    
    message->base.opcode = REMREQ_DELETE_VS_STATE;
    message->pipe = PIPE_HANDLE(_pipe);
    message->vs_handle = VS_HANDLE(state);
    
    enqueue_message(message);

}


static INLINE void
remote_context_set_blend_color(struct pipe_context *_pipe,
                              const struct pipe_blend_color *state)
{

    DBG("Set blend color\n");

    ALLOC_OUT_MESSAGE(set_blend_color, message);
    
    message->base.opcode = REMREQ_SET_BLEND_COLOR;
    message->pipe = PIPE_HANDLE(_pipe);
    message->state = *state;
    // Struct copy
    
    enqueue_message(message);

}


static INLINE void
remote_context_set_clip_state(struct pipe_context *_pipe,
                             const struct pipe_clip_state *state)
{

    DBG("Set clip state\n");

    ALLOC_OUT_MESSAGE(set_clip_state, message);
    
    message->base.opcode = REMREQ_SET_CLIP_STATE;
    message->pipe = PIPE_HANDLE(_pipe);
    message->state = *state;
    // Struct copy
    
    enqueue_message(message);

}


static INLINE void
remote_context_set_constant_buffer(struct pipe_context *_pipe,
                                  uint shader, uint index,
                                  const struct pipe_constant_buffer *buffer)
{

    DBG("Set constant buffer for shader %u, index %u\n", shader, index);

    ALLOC_OUT_MESSAGE(set_constant_buffer, message);
    
    message->base.opcode = REMREQ_SET_CONSTANT_BUFFER;
    message->pipe = PIPE_HANDLE(_pipe);
    message->shader = shader;
    message->index = index;
    message->buffer_size = buffer->size;
    message->buffer = BUFFER_HANDLE(buffer->buffer);
    /*
    char* mapped = (char*)pipe_buffer_map(_pipe->screen, buffer->buffer, PIPE_BUFFER_USAGE_CPU_READ);

    printf("On setting, constant buffer goes like this:\n");
    
    int i;
    for(i = 0; i < buffer->size; i++) {
      printf("%4d ", (int)mapped[i]);
      if(i % 8 == 7)
	printf("\n");
    }
    printf("\n");

    pipe_buffer_unmap(_pipe->screen, buffer->buffer);
    */
    enqueue_message(message);

}


static INLINE void
remote_context_set_framebuffer_state(struct pipe_context *_pipe,
                                    const struct pipe_framebuffer_state *state)
{
    unsigned i;
    ALLOC_OUT_MESSAGE(set_framebuffer_state, message);

    message->base.opcode = REMREQ_SET_FRAMEBUFFER_STATE;
    message->pipe = PIPE_HANDLE(_pipe);
    message->fbwidth = state->width;
    message->fbheight = state->height;
    message->fbnum_cbufs = state->num_cbufs;
    message->fbzsbuf = state->zsbuf ? SURFACE_HANDLE(state->zsbuf) : 0;
    for(i = 0; i < PIPE_MAX_COLOR_BUFS; i++)
	message->fbcbufs[i] = state->cbufs[i] ? SURFACE_HANDLE(state->cbufs[i]) : 0;
	
    DBG("Set framebuffer state: %dx%d\n", state->width, state->height);
    if(message->fbzsbuf) {
	DBG("Z/stencil buffer: %u\n", message->fbzsbuf);
    }
    else {
	DBG("No Z/stencil buffer\n");
    }
    
    for(i = 0; i < PIPE_MAX_COLOR_BUFS; i++)
	if(message->fbcbufs[i])
	    DBG("Colour buffer %d: texture %u\n", i, message->fbcbufs[i]);

    enqueue_message(message);

    struct remote_context* rctx = (struct remote_context*)_pipe;

    rctx->current_framebuffer_state = *state; // Struct copy
    
}


static INLINE void
remote_context_set_polygon_stipple(struct pipe_context *_pipe,
                                  const struct pipe_poly_stipple *state)
{

    DBG("Set polygon stipple\n");

    ALLOC_OUT_MESSAGE(set_polygon_stipple, message);
    
    message->base.opcode = REMREQ_SET_POLYGON_STIPPLE;
    message->pipe = PIPE_HANDLE(_pipe);
    message->state = *state;
    // Struct copy
    
    enqueue_message(message);
}


static INLINE void
remote_context_set_scissor_state(struct pipe_context *_pipe,
                                const struct pipe_scissor_state *state)
{

    DBG("Set scissor state\n");

    ALLOC_OUT_MESSAGE(set_scissor_state, message);
    
    message->base.opcode = REMREQ_SET_SCISSOR_STATE;
    message->pipe = PIPE_HANDLE(_pipe);
    message->state = *state;
    // Struct copy
    
    enqueue_message(message);
}


static INLINE void
remote_context_set_viewport_state(struct pipe_context *_pipe,
                                 const struct pipe_viewport_state *state)
{

    DBG("Set viewport state\n");

    ALLOC_OUT_MESSAGE(set_viewport_state, message);
    
    message->base.opcode = REMREQ_SET_VIEWPORT_STATE;
    message->pipe = PIPE_HANDLE(_pipe);
    message->state = *state;
    // Struct copy
    
    enqueue_message(message);
}


static INLINE void
remote_context_set_sampler_textures(struct pipe_context *_pipe,
                                   unsigned num_textures,
                                   struct pipe_texture **textures)
{
    // Similar to below
    
    DBG("Set sampler textures\n");
    
    struct remreq_set_sampler_textures* header = 
	(struct remreq_set_sampler_textures*)
	  allocate_message_memory(
	    sizeof(struct remreq_set_sampler_textures) + 
	    num_textures * sizeof(uint32_t));
	    
    header->base.opcode = REMREQ_SET_SAMPLER_TEXTURES;
    header->pipe = PIPE_HANDLE(_pipe);
    header->num_textures = num_textures;
    
    uint32_t* texhandles = (uint32_t*)
      ((char*)header + sizeof(struct remreq_set_sampler_textures));
    
    unsigned i;
    
    for(i = 0; i < num_textures; i++) {
	texhandles[i] = textures[i] ? TEXTURE_HANDLE(textures[i]) : 0;
	DBG("Sampler slot %d: texture %u\n", i, texhandles[i]);
    }

    enqueue_message(header);

}


static INLINE void
remote_context_set_vertex_buffers(struct pipe_context *_pipe,
                                 unsigned num_buffers,
                                 const struct pipe_vertex_buffer *buffers)
{
   // Each vertex_buffer points to a pipe_buffer, which here is actually
   // a remote_pipe_buffer. Flatten those references and send an array
   // which gives the buffer by opaque handle
   
  DBG("Set vertex buffers: %d buffers\n", num_buffers);
   
    struct remreq_set_vertex_buffers* header = 
	(struct remreq_set_vertex_buffers*)
	  allocate_message_memory(
	    sizeof(struct remreq_set_vertex_buffers) + 
	    (num_buffers * sizeof(struct opaque_pipe_vertex_element)));

    PANIC_IF_NULL(header);

    header->base.opcode = REMREQ_SET_VERTEX_BUFFERS;
    header->pipe = PIPE_HANDLE(_pipe);
    header->num_buffers = num_buffers;
    
    struct opaque_pipe_vertex_element* data =
	(struct opaque_pipe_vertex_element*)
      (((char*)header) + sizeof(struct remreq_set_vertex_buffers));
    
    unsigned i;
    
    for(i = 0; i < num_buffers; i++) {
	data[i].pitch = buffers[i].pitch;
	data[i].max_index = buffers[i].max_index;
	data[i].buffer_offset = buffers[i].buffer_offset;
	data[i].buffer = buffers[i].buffer ? BUFFER_HANDLE(buffers[i].buffer) : 0;
	DBG("Buffer slot %d assigned buffer handle %u\n", i, data[i].buffer);
    }
    
    enqueue_message(header);   

}


static INLINE void
remote_context_set_vertex_elements(struct pipe_context *_pipe,
                                  unsigned num_elements,
                                  const struct pipe_vertex_element *elements)
{
    // Send array
    
    DBG("Set vertex elements: %u elements\n", num_elements);
    
    struct remreq_set_vertex_elements* header = 
	(struct remreq_set_vertex_elements*)
	  allocate_message_memory(
	    sizeof(struct remreq_set_vertex_elements) + 
	    num_elements * sizeof(struct pipe_vertex_element));

    header->base.opcode = REMREQ_SET_VERTEX_ELEMENTS;
    header->pipe = PIPE_HANDLE(_pipe);
    header->num_elements = num_elements;
    
    char* dest = ((char*)header) + sizeof(struct remreq_set_vertex_elements);
    memcpy(dest, (void*)elements, num_elements * sizeof(struct pipe_vertex_element));	    
    
    enqueue_message(header);

}


static INLINE void
remote_context_surface_copy(struct pipe_context *_pipe,
                           boolean do_flip,
                           struct pipe_surface *dest,
                           unsigned destx, unsigned desty,
                           struct pipe_surface *src,
                           unsigned srcx, unsigned srcy,
                           unsigned width, unsigned height)
{

    DBG("Surface copy: %u-%ux%u-%ux%u --> %u-%ux%u\n", SURFACE_HANDLE(src), srcx, srcy, srcx + width, srcy + height, SURFACE_HANDLE(dest), destx, desty);

    ALLOC_OUT_MESSAGE(surface_copy, message);
    
    message->base.opcode = REMREQ_SURFACE_COPY;
    message->pipe = PIPE_HANDLE(_pipe);
    message->src = SURFACE_HANDLE(src);
    message->dest = SURFACE_HANDLE(dest);
    message->sx = srcx;
    message->sy = srcy;
    message->dx = destx;
    message->dy = desty;
    message->w = width;
    message->h = height;
    message->do_flip = do_flip;
    
    enqueue_message(message);

}


static INLINE void
remote_context_surface_fill(struct pipe_context *_pipe,
                           struct pipe_surface *dst,
                           unsigned dstx, unsigned dsty,
                           unsigned width, unsigned height,
                           unsigned value)
{

   DBG("Surface fill: %u-%ux%u-%ux%u with %x\n", PIPE_HANDLE(dst), dstx, dsty, dstx + width, dsty + height, value);

   ALLOC_OUT_MESSAGE(surface_fill, message);
   
   message->base.opcode = REMREQ_SURFACE_FILL;
   message->pipe = PIPE_HANDLE(_pipe);
   message->surface = SURFACE_HANDLE(dst);
   message->x = dstx;
   message->y = dsty;
   message->w = width;
   message->h = height;
   message->value = value;
   
   enqueue_message(message);

}


static INLINE void
remote_context_clear(struct pipe_context *_pipe, 
                    struct pipe_surface *surface,
                    unsigned clearValue)
{

   DBG("Clear surface %u with %x\n", SURFACE_HANDLE(surface), clearValue);

   ALLOC_OUT_MESSAGE(clear, message);
   
   message->base.opcode = REMREQ_CLEAR;
   message->pipe = PIPE_HANDLE(_pipe);
   message->surface = SURFACE_HANDLE(surface);
   message->clearValue = clearValue;
   
   enqueue_message(message);

}


static INLINE void
remote_context_flush(struct pipe_context *_pipe,
                    unsigned flags,
                    struct pipe_fence_handle **fence)
{

    DBG("Flush\n");

    ALLOC_OUT_MESSAGE(flush, message);

    message->base.opcode = REMREQ_FLUSH;
    message->pipe = PIPE_HANDLE(_pipe);
    message->flags = flags;

    /* !!! I *think* fence is out-only */

    enqueue_message(message);

    if(fence) *fence = NULL;
    
}


static INLINE void
remote_context_destroy(struct pipe_context *_pipe)
{
   ALLOC_OUT_MESSAGE(destroy_context, message);
   
   DBG("Destroying context %u\n", PIPE_HANDLE(_pipe));
   
   message->base.opcode = REMREQ_DESTROY_CONTEXT;
   message->pipe = PIPE_HANDLE(_pipe);
   
   enqueue_message(message);
   
   FREE((struct remote_context*)_pipe);
}


struct pipe_context *
remote_context_create(struct pipe_screen *screen)
{
   struct remote_context *tr_ctx;
   
   DBG("Creating new context for screen %u\n", SCREEN_HANDLE(screen));
   
   tr_ctx = CALLOC_STRUCT(remote_context);
   if(!tr_ctx)
      goto error1;

   tr_ctx->base.winsys = screen->winsys;
   tr_ctx->base.screen = screen;
   tr_ctx->base.destroy = remote_context_destroy;
   tr_ctx->base.set_edgeflags = remote_context_set_edgeflags;
   tr_ctx->base.draw_arrays = remote_context_draw_arrays;
   tr_ctx->base.draw_elements = remote_context_draw_elements;
   tr_ctx->base.draw_range_elements = remote_context_draw_range_elements;
   tr_ctx->base.create_query = remote_context_create_query;
   tr_ctx->base.destroy_query = remote_context_destroy_query;
   tr_ctx->base.begin_query = remote_context_begin_query;
   tr_ctx->base.end_query = remote_context_end_query;
   tr_ctx->base.get_query_result = remote_context_get_query_result;
   tr_ctx->base.create_blend_state = remote_context_create_blend_state;
   tr_ctx->base.bind_blend_state = remote_context_bind_blend_state;
   tr_ctx->base.delete_blend_state = remote_context_delete_blend_state;
   tr_ctx->base.create_sampler_state = remote_context_create_sampler_state;
   tr_ctx->base.bind_sampler_states = remote_context_bind_sampler_states;
   tr_ctx->base.delete_sampler_state = remote_context_delete_sampler_state;
   tr_ctx->base.create_rasterizer_state = remote_context_create_rasterizer_state;
   tr_ctx->base.bind_rasterizer_state = remote_context_bind_rasterizer_state;
   tr_ctx->base.delete_rasterizer_state = remote_context_delete_rasterizer_state;
   tr_ctx->base.create_depth_stencil_alpha_state = remote_context_create_depth_stencil_alpha_state;
   tr_ctx->base.bind_depth_stencil_alpha_state = remote_context_bind_depth_stencil_alpha_state;
   tr_ctx->base.delete_depth_stencil_alpha_state = remote_context_delete_depth_stencil_alpha_state;
   tr_ctx->base.create_fs_state = remote_context_create_fs_state;
   tr_ctx->base.bind_fs_state = remote_context_bind_fs_state;
   tr_ctx->base.delete_fs_state = remote_context_delete_fs_state;
   tr_ctx->base.create_vs_state = remote_context_create_vs_state;
   tr_ctx->base.bind_vs_state = remote_context_bind_vs_state;
   tr_ctx->base.delete_vs_state = remote_context_delete_vs_state;
   tr_ctx->base.set_blend_color = remote_context_set_blend_color;
   tr_ctx->base.set_clip_state = remote_context_set_clip_state;
   tr_ctx->base.set_constant_buffer = remote_context_set_constant_buffer;
   tr_ctx->base.set_framebuffer_state = remote_context_set_framebuffer_state;
   tr_ctx->base.set_polygon_stipple = remote_context_set_polygon_stipple;
   tr_ctx->base.set_scissor_state = remote_context_set_scissor_state;
   tr_ctx->base.set_viewport_state = remote_context_set_viewport_state;
   tr_ctx->base.set_sampler_textures = remote_context_set_sampler_textures;
   tr_ctx->base.set_vertex_buffers = remote_context_set_vertex_buffers;
   tr_ctx->base.set_vertex_elements = remote_context_set_vertex_elements;
   tr_ctx->base.surface_copy = remote_context_surface_copy;
   tr_ctx->base.surface_fill = remote_context_surface_fill;
   tr_ctx->base.clear = remote_context_clear;
   tr_ctx->base.flush = remote_context_flush;

   memset(&tr_ctx->current_framebuffer_state, 0, sizeof(struct pipe_framebuffer_state));

   ALLOC_OUT_MESSAGE(create_context, message);
   
   struct remote_screen* remscreen = (struct remote_screen*)screen;

   message->base.opcode = REMREQ_CREATE_CONTEXT;
   message->screen = remscreen->remote_handle;
   
   QUEUE_AND_WAIT(message, create_context, reply);

   if(reply) {
    DBG("Got reply: assigned handle %u\n", reply->handle);
    tr_ctx->remote_handle = reply->handle;
    free_message(reply);
    return &tr_ctx->base;
   }
   else {
    return NULL;
   }
   
error1:
   return NULL;
}


#include "remote_messages.h"

#include "client.h"
#include "dispatch.h"
#include "dispatch_functions.h"

#ifdef CS_DEBUG_DISPATCH
#include <stdio.h>
#include "debug.h"
#define DBG(format, args...) printf(format, ## args)
#else
#define DBG(format, args...)
#endif

int get_message_size(char* data) {

  return ((struct message_header*)data)->length;

}

int dispatch_message(struct client_list_entry* client, char* data) {

  struct message_header* header = (struct message_header*)data;
  
  char* optext = optotext(header->opcode);

  DBG("Dispatcher: dispatching for %s (length %u)\n", optext, header->length);

  switch(header->opcode) {

  case REMREQ_SET_EDGEFLAGS:
    {
      struct remreq_set_edgeflags* msg = (struct remreq_set_edgeflags*)header;
      dispatch_remreq_set_edgeflags(client, msg);
      break;
    }

  case REMREQ_DRAW_ARRAYS:
    {
      struct remreq_draw_arrays* msg = (struct remreq_draw_arrays*)header;
      dispatch_remreq_draw_arrays(client, msg);
      break;
    }

  case REMREQ_DRAW_ELEMENTS:
    {
      struct remreq_draw_elements* msg = (struct remreq_draw_elements*)header;
      dispatch_remreq_draw_elements(client, msg);
      break;
    }

  case REMREQ_DRAW_RANGE_ELEMENTS:
    {
      struct remreq_draw_range_elements* msg = (struct remreq_draw_range_elements*)header;
      dispatch_remreq_draw_range_elements(client, msg);
      break;
    }

  case REMREQ_CREATE_QUERY:
    {
      struct remreq_create_query* msg = (struct remreq_create_query*)header;
      dispatch_remreq_create_query(client, msg);
      break;
    }

  case REMREQ_DESTROY_QUERY:
    {
      struct remreq_destroy_query* msg = (struct remreq_destroy_query*)header;
      dispatch_remreq_destroy_query(client, msg);
      break;
    }

  case REMREQ_BEGIN_QUERY:
    {
      struct remreq_begin_query* msg = (struct remreq_begin_query*)header;
      dispatch_remreq_begin_query(client, msg);
      break;
    }

  case REMREQ_END_QUERY:
    {
      struct remreq_end_query* msg = (struct remreq_end_query*)header;
      dispatch_remreq_end_query(client, msg);
      break;
    }

  case REMREQ_GET_QUERY_RESULT:
    {
      struct remreq_get_query_result* msg = (struct remreq_get_query_result*)header;
      dispatch_remreq_get_query_result(client, msg);
      break;
    }

  case REMREQ_CREATE_BLEND_STATE:
    {
      struct remreq_create_blend_state* msg = (struct remreq_create_blend_state*)header;
      dispatch_remreq_create_blend_state(client, msg);
      break;
    }

  case REMREQ_BIND_BLEND_STATE:
    {
      struct remreq_bind_blend_state* msg = (struct remreq_bind_blend_state*)header;
      dispatch_remreq_bind_blend_state(client, msg);
      break;
    }

  case REMREQ_DELETE_BLEND_STATE:
    {
      struct remreq_delete_blend_state* msg = (struct remreq_delete_blend_state*)header;
      dispatch_remreq_delete_blend_state(client, msg);
      break;
    }

  case REMREQ_CREATE_SAMPLER_STATE:
    {
      struct remreq_create_sampler_state* msg = (struct remreq_create_sampler_state*)header;
      dispatch_remreq_create_sampler_state(client, msg);
      break;
    }

  case REMREQ_BIND_SAMPLER_STATE:
    {
      struct remreq_bind_sampler_state* msg = (struct remreq_bind_sampler_state*)header;
      dispatch_remreq_bind_sampler_state(client, msg);
      break;
    }

  case REMREQ_DELETE_SAMPLER_STATE:
    {
      struct remreq_delete_sampler_state* msg = (struct remreq_delete_sampler_state*)header;
      dispatch_remreq_delete_sampler_state(client, msg);
      break;
    }

  case REMREQ_CREATE_RAST_STATE:
    {
      struct remreq_create_rast_state* msg = (struct remreq_create_rast_state*)header;
      dispatch_remreq_create_rast_state(client, msg);
      break;
    }

  case REMREQ_BIND_RAST_STATE:
    {
      struct remreq_bind_rast_state* msg = (struct remreq_bind_rast_state*)header;
      dispatch_remreq_bind_rast_state(client, msg);
      break;
    }

  case REMREQ_DELETE_RAST_STATE:
    {
      struct remreq_delete_rast_state* msg = (struct remreq_delete_rast_state*)header;
      dispatch_remreq_delete_rast_state(client, msg);
      break;
    }

  case REMREQ_CREATE_DSA_STATE:
    {
      struct remreq_create_dsa_state* msg = (struct remreq_create_dsa_state*)header;
      dispatch_remreq_create_dsa_state(client, msg);
      break;
    }

  case REMREQ_BIND_DSA_STATE:
    {
      struct remreq_bind_dsa_state* msg = (struct remreq_bind_dsa_state*)header;
      dispatch_remreq_bind_dsa_state(client, msg);
      break;
    }

  case REMREQ_DELETE_DSA_STATE:
    {
      struct remreq_delete_dsa_state* msg = (struct remreq_delete_dsa_state*)header;
      dispatch_remreq_delete_dsa_state(client, msg);
      break;
    }

  case REMREQ_CREATE_FS_STATE:
    {
      struct remreq_create_fs_state* msg = (struct remreq_create_fs_state*)header;
      dispatch_remreq_create_fs_state(client, msg);
      break;
    }

  case REMREQ_BIND_FS_STATE:
    {
      struct remreq_bind_fs_state* msg = (struct remreq_bind_fs_state*)header;
      dispatch_remreq_bind_fs_state(client, msg);
      break;
    }

  case REMREQ_DELETE_FS_STATE:
    {
      struct remreq_delete_fs_state* msg = (struct remreq_delete_fs_state*)header;
      dispatch_remreq_delete_fs_state(client, msg);
      break;
    }

  case REMREQ_CREATE_VS_STATE:
    {
      struct remreq_create_vs_state* msg = (struct remreq_create_vs_state*)header;
      dispatch_remreq_create_vs_state(client, msg);
      break;
    }

  case REMREQ_BIND_VS_STATE:
    {
      struct remreq_bind_vs_state* msg = (struct remreq_bind_vs_state*)header;
      dispatch_remreq_bind_vs_state(client, msg);
      break;
    }

  case REMREQ_DELETE_VS_STATE:
    {
      struct remreq_delete_vs_state* msg = (struct remreq_delete_vs_state*)header;
      dispatch_remreq_delete_vs_state(client, msg);
      break;
    }

  case REMREQ_SET_BLEND_COLOR:
    {
      struct remreq_set_blend_color* msg = (struct remreq_set_blend_color*)header;
      dispatch_remreq_set_blend_color(client, msg);
      break;
    }

  case REMREQ_SET_CLIP_STATE:
    {
      struct remreq_set_clip_state* msg = (struct remreq_set_clip_state*)header;
      dispatch_remreq_set_clip_state(client, msg);
      break;
    }

  case REMREQ_SET_POLYGON_STIPPLE:
    {
      struct remreq_set_polygon_stipple* msg = (struct remreq_set_polygon_stipple*)header;
      dispatch_remreq_set_polygon_stipple(client, msg);
      break;
    }

  case REMREQ_SET_SCISSOR_STATE:
    {
      struct remreq_set_scissor_state* msg = (struct remreq_set_scissor_state*)header;
      dispatch_remreq_set_scissor_state(client, msg);
      break;
    }

  case REMREQ_SET_VIEWPORT_STATE:
    {
      struct remreq_set_viewport_state* msg = (struct remreq_set_viewport_state*)header;
      dispatch_remreq_set_viewport_state(client, msg);
      break;
    }

  case REMREQ_SET_CONSTANT_BUFFER:
    {
      struct remreq_set_constant_buffer* msg = (struct remreq_set_constant_buffer*)header;
      dispatch_remreq_set_constant_buffer(client, msg);
      break;
    }

  case REMREQ_SET_FRAMEBUFFER_STATE:
    {
      struct remreq_set_framebuffer_state* msg = (struct remreq_set_framebuffer_state*)header;
      dispatch_remreq_set_framebuffer_state(client, msg);
      break;
    }

  case REMREQ_SET_SAMPLER_TEXTURES:
    {
      struct remreq_set_sampler_textures* msg = (struct remreq_set_sampler_textures*)header;
      dispatch_remreq_set_sampler_textures(client, msg);
      break;
    }

  case REMREQ_SET_VERTEX_BUFFERS:
    {
      struct remreq_set_vertex_buffers* msg = (struct remreq_set_vertex_buffers*)header;
      dispatch_remreq_set_vertex_buffers(client, msg);
      break;
    }

  case REMREQ_SET_VERTEX_ELEMENTS:
    {
      struct remreq_set_vertex_elements* msg = (struct remreq_set_vertex_elements*)header;
      dispatch_remreq_set_vertex_elements(client, msg);
      break;
    }

  case REMREQ_SURFACE_COPY:
    {
      struct remreq_surface_copy* msg = (struct remreq_surface_copy*)header;
      dispatch_remreq_surface_copy(client, msg);
      break;
    }

  case REMREQ_SURFACE_FILL:
    {
      struct remreq_surface_fill* msg = (struct remreq_surface_fill*)header;
      dispatch_remreq_surface_fill(client, msg);
      break;
    }

  case REMREQ_CLEAR:
    {
      struct remreq_clear* msg = (struct remreq_clear*)header;
      dispatch_remreq_clear(client, msg);
      break;
    }

  case REMREQ_FLUSH:
    {
      struct remreq_flush* msg = (struct remreq_flush*)header;
      dispatch_remreq_flush(client, msg);
      break;
    }

  case REMREQ_DESTROY_CONTEXT:
    {
      struct remreq_destroy_context* msg = (struct remreq_destroy_context*)header;
      dispatch_remreq_destroy_context(client, msg);
      break;
    }

  case REMREQ_CREATE_CONTEXT:
    {
      struct remreq_create_context* msg = (struct remreq_create_context*)header;
      dispatch_remreq_create_context(client, msg);
      break;
    }

  case REMREQ_GET_PARAM:
    {
      struct remreq_get_param* msg = (struct remreq_get_param*)header;
      dispatch_remreq_get_param(client, msg);
      break;
    }

  case REMREQ_GET_PARAMF:
    {
      struct remreq_get_paramf* msg = (struct remreq_get_paramf*)header;
      dispatch_remreq_get_paramf(client, msg);
      break;
    }

  case REMREQ_IS_FORMAT_SUPPORTED:
    {
      struct remreq_is_format_supported* msg = (struct remreq_is_format_supported*)header;
      dispatch_remreq_is_format_supported(client, msg);
      break;
    }

  case REMREQ_TEXTURE_CREATE:
    {
      struct remreq_texture_create* msg = (struct remreq_texture_create*)header;
      dispatch_remreq_texture_create(client, msg);
      break;
    }

  case REMREQ_TEXTURE_BLANKET:
    {
      struct remreq_texture_blanket* msg = (struct remreq_texture_blanket*)header;
      dispatch_remreq_texture_blanket(client, msg);
      break;
    }

  case REMREQ_TEXTURE_RELEASE:
    {
      struct remreq_texture_release* msg = (struct remreq_texture_release*)header;
      dispatch_remreq_texture_release(client, msg);
      break;
    }

  case REMREQ_GET_TEX_SURFACE:
    {
      struct remreq_get_tex_surface* msg = (struct remreq_get_tex_surface*)header;
      dispatch_remreq_get_tex_surface(client, msg);
      break;
    }

  case REMREQ_TEX_SURFACE_RELEASE:
    {
      struct remreq_tex_surface_release* msg = (struct remreq_tex_surface_release*)header;
      dispatch_remreq_tex_surface_release(client, msg);
      break;
    }

  case REMREQ_BUFFER_GET_DATA:
    {
      struct remreq_buffer_get_data* msg = (struct remreq_buffer_get_data*)header;
      dispatch_remreq_buffer_get_data(client, msg);
      break;
    }

  case REMREQ_BUFFER_SET_DATA:
    {
      struct remreq_buffer_set_data* msg = (struct remreq_buffer_set_data*)header;
      dispatch_remreq_buffer_set_data(client, msg);
      break;
    }

  case REMREQ_CREATE_SCREEN:
    {
      struct remreq_create_screen* msg = (struct remreq_create_screen*)header;
      dispatch_remreq_create_screen(client, msg);
      break;
    }

  case REMREQ_FLUSH_FRONTBUFFER:
    {
      struct remreq_flush_frontbuffer* msg = (struct remreq_flush_frontbuffer*)header;
      dispatch_remreq_flush_frontbuffer(client, msg);
      break;
    }

  case REMREQ_BUFFER_CREATE:
    {
      struct remreq_buffer_create* msg = (struct remreq_buffer_create*)header;
      dispatch_remreq_buffer_create(client, msg);
      break;
    }

  case REMREQ_USER_BUFFER_CREATE:
    {
      struct remreq_user_buffer_create* msg = (struct remreq_user_buffer_create*)header;
      dispatch_remreq_user_buffer_create(client, msg);
      break;
    }

  case REMREQ_USER_BUFFER_UPDATE:
    {
      struct remreq_user_buffer_update* msg = (struct remreq_user_buffer_update*)header;
      dispatch_remreq_user_buffer_update(client, msg);
      break;
    }
    /*
  case REMREQ_BUFFER_MAP:
    {
      struct remreq_buffer_map* msg = (struct remreq_buffer_map*)header;
      dispatch_remreq_buffer_map(client, msg);
      break;
    }

  case REMREQ_BUFFER_UNMAP:
    {
      struct remreq_buffer_unmap* msg = (struct remreq_buffer_unmap*)header;
      dispatch_remreq_buffer_unmap(client, msg);
      break;
    }
    */
  case REMREQ_BUFFER_DESTROY:
    {
      struct remreq_buffer_destroy* msg = (struct remreq_buffer_destroy*)header;
      dispatch_remreq_buffer_destroy(client, msg);
      break;
    }

  case REMREQ_SWAP_BUFFERS:
    {
      struct remreq_swap_buffers* msg = (struct remreq_swap_buffers*)header;
      dispatch_remreq_swap_buffers(client, msg);
      break;
    }

  }

}

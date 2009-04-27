#ifndef XEN3D_DISPATCH_FUNCTIONS_H
#define XEN3D_DISPATCH_FUNCTIONS_H

	int dispatch_remreq_set_edgeflags(struct client_list_entry* client, struct remreq_set_edgeflags* message);

	int dispatch_remreq_draw_arrays(struct client_list_entry* client, struct remreq_draw_arrays* message);

	int dispatch_remreq_draw_elements(struct client_list_entry* client, struct remreq_draw_elements* message);

	int dispatch_remreq_draw_range_elements(struct client_list_entry* client, struct remreq_draw_range_elements* message);

	int dispatch_remreq_create_query(struct client_list_entry* client, struct remreq_create_query* message);

	int dispatch_remreq_destroy_query(struct client_list_entry* client, struct remreq_destroy_query* message);

	int dispatch_remreq_begin_query(struct client_list_entry* client, struct remreq_begin_query* message);

	int dispatch_remreq_end_query(struct client_list_entry* client, struct remreq_end_query* message);

	int dispatch_remreq_get_query_result(struct client_list_entry* client, struct remreq_get_query_result* message);

	int dispatch_remreq_create_blend_state(struct client_list_entry* client, struct remreq_create_blend_state* message);

	int dispatch_remreq_bind_blend_state(struct client_list_entry* client, struct remreq_bind_blend_state* message);

	int dispatch_remreq_delete_blend_state(struct client_list_entry* client, struct remreq_delete_blend_state* message);

	int dispatch_remreq_create_sampler_state(struct client_list_entry* client, struct remreq_create_sampler_state* message);

	int dispatch_remreq_bind_sampler_state(struct client_list_entry* client, struct remreq_bind_sampler_state* message);

	int dispatch_remreq_delete_sampler_state(struct client_list_entry* client, struct remreq_delete_sampler_state* message);

	int dispatch_remreq_create_rast_state(struct client_list_entry* client, struct remreq_create_rast_state* message);

	int dispatch_remreq_bind_rast_state(struct client_list_entry* client, struct remreq_bind_rast_state* message);

	int dispatch_remreq_delete_rast_state(struct client_list_entry* client, struct remreq_delete_rast_state* message);

	int dispatch_remreq_create_dsa_state(struct client_list_entry* client, struct remreq_create_dsa_state* message);

	int dispatch_remreq_bind_dsa_state(struct client_list_entry* client, struct remreq_bind_dsa_state* message);

	int dispatch_remreq_delete_dsa_state(struct client_list_entry* client, struct remreq_delete_dsa_state* message);

	int dispatch_remreq_create_fs_state(struct client_list_entry* client, struct remreq_create_fs_state* message);

	int dispatch_remreq_bind_fs_state(struct client_list_entry* client, struct remreq_bind_fs_state* message);

	int dispatch_remreq_delete_fs_state(struct client_list_entry* client, struct remreq_delete_fs_state* message);

	int dispatch_remreq_create_vs_state(struct client_list_entry* client, struct remreq_create_vs_state* message);

	int dispatch_remreq_bind_vs_state(struct client_list_entry* client, struct remreq_bind_vs_state* message);

	int dispatch_remreq_delete_vs_state(struct client_list_entry* client, struct remreq_delete_vs_state* message);

	int dispatch_remreq_set_blend_color(struct client_list_entry* client, struct remreq_set_blend_color* message);

	int dispatch_remreq_set_clip_state(struct client_list_entry* client, struct remreq_set_clip_state* message);

	int dispatch_remreq_set_polygon_stipple(struct client_list_entry* client, struct remreq_set_polygon_stipple* message);

	int dispatch_remreq_set_scissor_state(struct client_list_entry* client, struct remreq_set_scissor_state* message);

	int dispatch_remreq_set_viewport_state(struct client_list_entry* client, struct remreq_set_viewport_state* message);

	int dispatch_remreq_set_constant_buffer(struct client_list_entry* client, struct remreq_set_constant_buffer* message);

	int dispatch_remreq_set_framebuffer_state(struct client_list_entry* client, struct remreq_set_framebuffer_state* message);

	int dispatch_remreq_set_sampler_textures(struct client_list_entry* client, struct remreq_set_sampler_textures* message);

	int dispatch_remreq_set_vertex_buffers(struct client_list_entry* client, struct remreq_set_vertex_buffers* message);

	int dispatch_remreq_set_vertex_elements(struct client_list_entry* client, struct remreq_set_vertex_elements* message);

	int dispatch_remreq_surface_copy(struct client_list_entry* client, struct remreq_surface_copy* message);

	int dispatch_remreq_surface_fill(struct client_list_entry* client, struct remreq_surface_fill* message);

	int dispatch_remreq_clear(struct client_list_entry* client, struct remreq_clear* message);

	int dispatch_remreq_flush(struct client_list_entry* client, struct remreq_flush* message);

	int dispatch_remreq_destroy_context(struct client_list_entry* client, struct remreq_destroy_context* message);

	int dispatch_remreq_create_context(struct client_list_entry* client, struct remreq_create_context* message);

	int dispatch_remreq_get_param(struct client_list_entry* client, struct remreq_get_param* message);

	int dispatch_remreq_get_paramf(struct client_list_entry* client, struct remreq_get_paramf* message);

	int dispatch_remreq_is_format_supported(struct client_list_entry* client, struct remreq_is_format_supported* message);

	int dispatch_remreq_texture_create(struct client_list_entry* client, struct remreq_texture_create* message);

	int dispatch_remreq_texture_blanket(struct client_list_entry* client, struct remreq_texture_blanket* message);

	int dispatch_remreq_texture_release(struct client_list_entry* client, struct remreq_texture_release* message);

	int dispatch_remreq_get_tex_surface(struct client_list_entry* client, struct remreq_get_tex_surface* message);

	int dispatch_remreq_tex_surface_release(struct client_list_entry* client, struct remreq_tex_surface_release* message);

/*	int dispatch_remreq_surface_map(struct client_list_entry* client, struct remreq_surface_map* message);

	int dispatch_remreq_surface_unmap(struct client_list_entry* client, struct remreq_surface_unmap* message);*/

int dispatch_remreq_buffer_get_data(struct client_list_entry* client, struct remreq_buffer_get_data* message);

int dispatch_remreq_buffer_set_data(struct client_list_entry* client, struct remreq_buffer_set_data* message);

	int dispatch_remreq_create_screen(struct client_list_entry* client, struct remreq_create_screen* message);

	int dispatch_remreq_flush_frontbuffer(struct client_list_entry* client, struct remreq_flush_frontbuffer* message);

	int dispatch_remreq_buffer_create(struct client_list_entry* client, struct remreq_buffer_create* message);

	int dispatch_remreq_user_buffer_create(struct client_list_entry* client, struct remreq_user_buffer_create* message);

	int dispatch_remreq_user_buffer_update(struct client_list_entry* client, struct remreq_user_buffer_update* message);

/*	int dispatch_remreq_buffer_map(struct client_list_entry* client, struct remreq_buffer_map* message);

	int dispatch_remreq_buffer_unmap(struct client_list_entry* client, struct remreq_buffer_unmap* message);*/

	int dispatch_remreq_buffer_destroy(struct client_list_entry* client, struct remreq_buffer_destroy* message);

	int dispatch_remreq_swap_buffers(struct client_list_entry* client, struct remreq_swap_buffers* message);

#endif

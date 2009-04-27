
#ifndef XEN3D_CLIENT_H
#define XEN3D_CLIENT_H

#include "map_list.h"
#include "picker.h"
#include "pipe/p_state.h"
#include "EGL/egl.h"

#include "main.h"
// For struct receiver

#include <xorg/regionstr.h>
#include <xorg/miscstruct.h>
// For BoxRec, BoxPtr

#include <X11/extensions/xen3d_extproto.h>
// For XVMGLWindowingCommand

#include <Hermes/Hermes.h>
// For HermesFormat, HermesHandle

struct client_context;

struct client_texture {

  struct pipe_texture* base;
  struct pipe_texture* frontbuffer;
  struct client_context* fbcontext;
  struct pipe_texture client_request;
  int client_size;

  HermesFormat* hermes_format;

};

struct client_surface {
  
  struct pipe_surface* base;
  struct client_texture* texture;

};

struct client_buffer {

  struct pipe_buffer* base;
  boolean remote_texture_layout_may_differ;
  struct client_texture* texture;

};

struct tgsi_token;

struct client_fs {

  void* base;
  struct tgsi_token* tokens;

};

struct client_vs {

  void* base;
  struct tgsi_token* tokens;

};

#define TRIVIAL_WRAP(x) struct client_##x { void* base; };

TRIVIAL_WRAP(blend);
TRIVIAL_WRAP(query);
TRIVIAL_WRAP(sampler);
TRIVIAL_WRAP(rast);
TRIVIAL_WRAP(dsa);

// Screen-wide maps

MAP_DECLARE(uint32_t, struct client_context*, contextmap);
MAP_DECLARE(uint32_t, struct client_texture*, texturemap);
MAP_DECLARE(uint32_t, struct client_surface*, surfacemap);
MAP_DECLARE(uint32_t, struct client_buffer*, buffermap);

// Per-context maps

MAP_DECLARE(uint32_t, struct client_blend*, blendmap);
MAP_DECLARE(uint32_t, struct client_query*, querymap);
MAP_DECLARE(uint32_t, struct client_sampler*, samplermap);
MAP_DECLARE(uint32_t, struct client_rast*, rastmap);
MAP_DECLARE(uint32_t, struct client_dsa*, dsamap);
MAP_DECLARE(uint32_t, struct client_fs*, fsmap);
MAP_DECLARE(uint32_t, struct client_vs*, vsmap);

struct client_context {

  EGLContext egl_ctx;
  struct pipe_context* pipe_ctx;

  //  boolean fbstate_valid;
  //  struct pipe_framebuffer_state fbstate;
  //  struct client_texture* ccbufs[PIPE_MAX_COLOR_BUFS];

  struct blendmap* blends;
  struct querymap* queries;
  struct samplermap* samplers;
  struct rastmap* rasts;
  struct dsamap* dsas;
  struct fsmap* fss;
  struct vsmap* vss;

};

struct drawable_surface_list {

  uint32_t screen;
  uint32_t window;
  int x, y;
  int w, h;
  int ncliprects;
  BoxPtr cliprects;  
  struct client_texture* texture;
  struct drawable_surface_list* next;

};

struct tgsi_token;

#define CONTROL_DOM_STATE_INVISIBLE 0
#define CONTROL_DOM_STATE_PENDING 1
#define CONTROL_DOM_STATE_VISIBLE 2

struct global_state {

  EGLContext current_egl_ctx;
  EGLContext master_egl_context;
  EGLSurface window_egl_surface;
  EGLDisplay egl_display;
  EGLConfig egl_config;
  int window_width, window_height, window_depth, window_pixel_size;
  enum pipe_format window_pipe_format;

  HermesHandle hermes_handle;
  HermesFormat* hermes_dest_format;
  
  Display* x_display;
  
  uint32_t screen_handles;

  struct tgsi_token* vs_tex_tokens;
  struct tgsi_token* fs_tex_tokens;

  struct pipe_screen* screen;
  
  struct domain_list_entry* current_domain;

  struct receiver domain_control_receiver;
  struct domain_list_entry** domain_list_head;
  int trusted_domain;
  struct domain_list_entry* trusted_domain_descriptor;

  int control_dom_state;
  int control_dom_off_x;
  int control_dom_off_y;
  int control_dom_n_clip_rects;
  struct xen3d_clip_rect* control_dom_clip_rects;

  int control_domain_enabled;

};

struct client_screen {

  struct contextmap* contexts;
  struct texturemap* textures;
  struct surfacemap* surfaces;
  struct buffermap* buffers;
  
  uint32_t remote_id;

  struct pipe_screen* screen;

  uint32_t ctx_handles;

};

struct client_list_entry {

  struct client_list_entry* next;
  int clientid;

  struct receiver receiver;

  struct client_screen* screen;

  struct global_state* global_state;
  struct domain_list_entry* domain;

};

struct domain_list_entry {

  struct domain_list_entry* next;
  int domain;
  struct global_state* global_state;
  struct client_list_entry* clients;
  struct receiver x_receiver;
  struct receiver qemu_receiver;
  
  struct drawable_surface_list* drawables;
  int qemu_sem_handle;
  void* qemu_shared_section;
  
  struct pipe_texture* backdrop_texture;
  
  int backdrop_width;
  int backdrop_height;
  int dirty_x;
  int dirty_y;
  int dirty_w;
  int dirty_h;
  
};

struct pipe_context;

struct pipe_context* get_as_current(struct client_context*, struct client_list_entry*);

#endif

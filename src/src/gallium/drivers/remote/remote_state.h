
#ifndef REMOTE_STATE_H
#define REMOTE_STATE_H

#include <pipe/p_state.h>

#define TRIVIAL_REMOTE(x) struct x { uint32_t handle; };

TRIVIAL_REMOTE(remote_opaque_sampler_state)
TRIVIAL_REMOTE(remote_pipe_query)
TRIVIAL_REMOTE(remote_opaque_blend_state)
TRIVIAL_REMOTE(remote_opaque_rast_state)
TRIVIAL_REMOTE(remote_opaque_dsa_state)
TRIVIAL_REMOTE(opaque_remote_fs)
TRIVIAL_REMOTE(opaque_remote_vs)

#undef TRIVIAL_REMOTE

struct remote_buffer {

  struct pipe_buffer base;
  unsigned is_user;
  unsigned nmaps;
  void* map;
  uint32_t handle;
  boolean local_dirty, remote_dirty;
    
};

struct remote_surface {

  struct pipe_surface base;
  uint32_t handle;
    
};

struct remote_texture {

  struct pipe_texture base;
  unsigned long stride[PIPE_MAX_TEXTURE_LEVELS];
  unsigned long level_offset[PIPE_MAX_TEXTURE_LEVELS];
  struct pipe_buffer* backing_buffer;
  uint32_t handle;
    
};

#endif

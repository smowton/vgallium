
#ifndef REMOTE_MESSAGES_H
#define REMOTE_MESSAGES_H

#include "pipe/p_state.h"
#include "pipe/p_compiler.h"

struct message_header {

    uint32_t opcode;
    uint32_t length;
    uint32_t xid;

};

struct remreq_set_edgeflags {

    struct message_header base;
    uint32_t pipe;
    unsigned flag;
  boolean isnull;
    
};

#define REMREQ_SET_EDGEFLAGS 1

struct remreq_draw_arrays {

    struct message_header base;
    uint32_t pipe;
    unsigned mode;
    unsigned start;
    unsigned count;
    
};

#define REMREQ_DRAW_ARRAYS 2

struct remrep_draw_arrays {

    struct message_header base;
    boolean success;

};

#define REMREP_DRAW_ARRAYS 73

struct remreq_draw_elements {

    struct message_header base;
    uint32_t pipe;
    uint32_t buffer;
    unsigned indexSize;
    unsigned mode;
    unsigned start;
    unsigned count;
    
};

#define REMREQ_DRAW_ELEMENTS 3

struct remrep_draw_elements {

    struct message_header base;
    boolean success;
    
};

#define REMREP_DRAW_ELEMENTS 74

struct remreq_draw_range_elements {

    struct message_header base;
    uint32_t pipe;
    uint32_t buffer;
    unsigned indexSize;
    unsigned mode;
    unsigned start;
    unsigned count;
    unsigned minIndex;
    unsigned maxIndex;
    
};

#define REMREQ_DRAW_RANGE_ELEMENTS 4

struct remrep_draw_range_elements {

    struct message_header base;
    boolean success;
    
};

#define REMREP_DRAW_RANGE_ELEMENTS 75

struct remreq_create_query {

    struct message_header base;
    uint32_t pipe;
    uint32_t handle;
    unsigned query_type;
    
};

#define REMREQ_CREATE_QUERY 5

struct remreq_destroy_query {

    struct message_header base;
    uint32_t pipe;
    uint32_t query;
    
};

#define REMREQ_DESTROY_QUERY 6

struct remreq_begin_query {

    struct message_header base;
    uint32_t pipe;
    uint32_t query;
    
};

#define REMREQ_BEGIN_QUERY 7

struct remreq_end_query {

    struct message_header base;
    uint32_t pipe;
    uint32_t query;
    
};

#define REMREQ_END_QUERY 8

struct remreq_get_query_result {

    struct message_header base;
    uint32_t pipe;
    uint32_t query;
    boolean wait;
    
};

#define REMREQ_GET_QUERY_RESULT 9

struct remrep_get_query_result {

    struct message_header base;
    uint64 result;
    boolean done;
    
};

#define REMREP_GET_QUERY_RESULT 10

struct remreq_create_blend_state {

    struct message_header base;
    uint32_t handle;
    uint32_t pipe;
    struct pipe_blend_state state;
    
};

#define REMREQ_CREATE_BLEND_STATE 11

struct remreq_bind_blend_state {

    struct message_header base;
    uint32_t pipe;
    uint32_t blend_handle;
    
};

#define REMREQ_BIND_BLEND_STATE 12

struct remreq_delete_blend_state {

    struct message_header base;
    uint32_t pipe;
    uint32_t blend_handle;
    
};

#define REMREQ_DELETE_BLEND_STATE 13

struct remreq_create_sampler_state {

    struct message_header base;
    uint32_t handle;
    uint32_t pipe;
    struct pipe_sampler_state state;
    
};

#define REMREQ_CREATE_SAMPLER_STATE 14

struct remreq_bind_sampler_state {

    struct message_header base;
    uint32_t pipe;
    uint32_t nstates;
    
};

#define REMREQ_BIND_SAMPLER_STATE 15

struct remreq_delete_sampler_state {

    struct message_header base;
    uint32_t pipe;
    uint32_t sampler_handle;
    
};

#define REMREQ_DELETE_SAMPLER_STATE 16

struct remreq_create_rast_state {

    struct message_header base;
    uint32_t handle;
    uint32_t pipe;
    struct pipe_rasterizer_state state;
    
};

#define REMREQ_CREATE_RAST_STATE 17

struct remreq_bind_rast_state {

    struct message_header base;
    uint32_t pipe;
    uint32_t rast_handle;
    
};

#define REMREQ_BIND_RAST_STATE 18

struct remreq_delete_rast_state {

    struct message_header base;
    uint32_t pipe;
    uint32_t rast_handle;
    
};

#define REMREQ_DELETE_RAST_STATE 19

struct remreq_create_dsa_state {

    struct message_header base;
    uint32_t handle;
    uint32_t pipe;
    struct pipe_depth_stencil_alpha_state state;
    
};

#define REMREQ_CREATE_DSA_STATE 20

struct remreq_bind_dsa_state {

    struct message_header base;
    uint32_t pipe;
    uint32_t dsa_handle;
    
};

#define REMREQ_BIND_DSA_STATE 21

struct remreq_delete_dsa_state {

    struct message_header base;
    uint32_t pipe;
    uint32_t dsa_handle;
    
};

#define REMREQ_DELETE_DSA_STATE 22

struct remreq_create_fs_state {

  struct message_header base;
  uint32_t pipe;
  uint32_t fs_handle;

};

#define REMREQ_CREATE_FS_STATE 23

struct remreq_bind_fs_state {

  struct message_header base;
  uint32_t pipe;
  uint32_t fs_handle;

};

#define REMREQ_BIND_FS_STATE 24

struct remreq_delete_fs_state {

  struct message_header base;
  uint32_t pipe;
  uint32_t fs_handle;

};

#define REMREQ_DELETE_FS_STATE 25

struct remreq_create_vs_state {

  struct message_header base;
  uint32_t pipe;
  uint32_t vs_handle;

};

#define REMREQ_CREATE_VS_STATE 26

struct remreq_bind_vs_state {

  struct message_header base;
  uint32_t pipe;
  uint32_t vs_handle;

};

#define REMREQ_BIND_VS_STATE 27

struct remreq_delete_vs_state {

  struct message_header base;
  uint32_t pipe;
  uint32_t vs_handle;

};

#define REMREQ_DELETE_VS_STATE 28

struct remreq_set_blend_color {

  struct message_header base;
  uint32_t pipe;
  struct pipe_blend_color state;

};

#define REMREQ_SET_BLEND_COLOR 29

struct remreq_set_clip_state {

  struct message_header base;
  uint32_t pipe;
  struct pipe_clip_state state;

};

#define REMREQ_SET_CLIP_STATE 30

struct remreq_set_polygon_stipple {

  struct message_header base;
  uint32_t pipe;
  struct pipe_poly_stipple state;

};

#define REMREQ_SET_POLYGON_STIPPLE 31

struct remreq_set_scissor_state {

  struct message_header base;
  uint32_t pipe;
  struct pipe_scissor_state state;

};

#define REMREQ_SET_SCISSOR_STATE 32

struct remreq_set_viewport_state {

  struct message_header base;
  uint32_t pipe;
  struct pipe_viewport_state state;

};

#define REMREQ_SET_VIEWPORT_STATE 33

struct remreq_set_constant_buffer {

  struct message_header base;
  uint32_t pipe;
  unsigned int shader, index;
  unsigned buffer_size;
  uint32_t buffer;

};

#define REMREQ_SET_CONSTANT_BUFFER 34

struct remreq_set_framebuffer_state {

  struct message_header base;
  uint32_t pipe;
  unsigned fbwidth, fbheight;
  unsigned fbnum_cbufs;
  uint32_t fbzsbuf;
  uint32_t fbcbufs[PIPE_MAX_COLOR_BUFS];

};

#define REMREQ_SET_FRAMEBUFFER_STATE 35

struct remreq_set_sampler_textures {

  struct message_header base;
  uint32_t pipe;
  unsigned num_textures;

};

#define REMREQ_SET_SAMPLER_TEXTURES 36

struct opaque_pipe_vertex_element {

  unsigned pitch, max_index, buffer_offset;
  uint32_t buffer;

};

struct remreq_set_vertex_buffers {

  struct message_header base;
  uint32_t pipe;
  unsigned num_buffers;

};

#define REMREQ_SET_VERTEX_BUFFERS 37

struct remreq_set_vertex_elements {

  struct message_header base;
  uint32_t pipe;
  unsigned num_elements;

};

#define REMREQ_SET_VERTEX_ELEMENTS 38

struct remreq_surface_copy {

  struct message_header base;
  uint32_t pipe;
  uint32_t src, dest;
  unsigned sx, sy, dx, dy;
  unsigned w, h;
  boolean do_flip;

};

#define REMREQ_SURFACE_COPY 39

struct remreq_surface_fill {

  struct message_header base;
  uint32_t pipe;
  uint32_t surface;
  unsigned x, y;
  unsigned w, h;
  unsigned value;

};

#define REMREQ_SURFACE_FILL 40

struct remreq_clear {

  struct message_header base;
  uint32_t pipe;
  uint32_t surface;
  unsigned clearValue;

};

#define REMREQ_CLEAR 41

struct remreq_flush {

  struct message_header base;
  uint32_t pipe;
  unsigned flags;

};

#define REMREQ_FLUSH 42

struct remreq_destroy_context {

  struct message_header base;
  uint32_t pipe;

};

#define REMREQ_DESTROY_CONTEXT 44

struct remreq_create_context {

  struct message_header base;
  uint32_t screen;

};

#define REMREQ_CREATE_CONTEXT 45

struct remrep_create_context {

  struct message_header base;
  uint32_t handle;

};

#define REMREP_CREATE_CONTEXT 46

// Screen-wide messages

struct remreq_get_param {

  struct message_header base;
  uint32_t screen;
  int param;

};

#define REMREQ_GET_PARAM 47

struct remrep_get_param {

  struct message_header base;
  int response;

};

#define REMREP_GET_PARAM 48

struct remreq_get_paramf {

  struct message_header base;
  uint32_t screen;
  float param;

};

#define REMREQ_GET_PARAMF 49

struct remrep_get_paramf {

  struct message_header base;
  float response;

};

#define REMREP_GET_PARAMF 50

struct remreq_is_format_supported {

  struct message_header base;
  uint32_t screen;
  enum pipe_format format;
  enum pipe_texture_target target;
  unsigned tex_usage, geom_flags;

};

#define REMREQ_IS_FORMAT_SUPPORTED 51

struct remrep_is_format_supported {

  struct message_header base;
  boolean response;

};

#define REMREP_IS_FORMAT_SUPPORTED 52

struct remreq_texture_create {

  struct message_header base;
  uint32_t screen;
  struct pipe_texture templat;
  uint32_t handle;

};

#define REMREQ_TEXTURE_CREATE 53

struct remrep_texture_create {

  struct message_header base;
  boolean success;

};

#define REMREP_TEXTURE_CREATE 54

struct remreq_texture_blanket {

  struct message_header base;
  uint32_t screen;
  struct pipe_texture templat;
  uint32_t handle;
  unsigned pitch;
  uint32_t buffer;

};

#define REMREQ_TEXTURE_BLANKET 55

struct remrep_texture_blanket {

  struct message_header base;
  boolean success;

};

#define REMREP_TEXTURE_BLANKET 56

struct remreq_texture_release {

  struct message_header base;
  uint32_t screen;
  uint32_t texture;

};

#define REMREQ_TEXTURE_RELEASE 57

struct remreq_get_tex_surface {

  struct message_header base;
  uint32_t screen;
  uint32_t texture;
  unsigned face, level, zslice, usage;
  uint32_t handle;
  uint32_t buffer_handle;

};

#define REMREQ_GET_TEX_SURFACE 58

struct remrep_get_tex_surface {

  struct message_header base;
  boolean success;

};

#define REMREP_GET_TEX_SURFACE 59

struct remreq_tex_surface_release {

  struct message_header base;
  uint32_t screen;
  uint32_t surface;

};

#define REMREQ_TEX_SURFACE_RELEASE 60

/*
struct remreq_surface_map {

  struct message_header base;
  uint32_t screen;
  uint32_t surface;
  boolean map_ro;

};

#define REMREQ_SURFACE_MAP 61

struct remrep_surface_map {

  struct message_header base;
  boolean success;
  unsigned nbytes;

};

#define REMREP_SURFACE_MAP 62

struct remreq_surface_unmap {

  struct message_header base;
  uint32_t screen;
  uint32_t surface;
  boolean has_data;

};

#define REMREQ_SURFACE_UNMAP 63
*/

struct remreq_buffer_get_data {
 
  struct message_header base;
  uint32_t winsys;
  uint32_t buffer;

};

#define REMREQ_BUFFER_GET_DATA 61

struct remrep_buffer_get_data {

  struct message_header base;
  boolean success;

};

#define REMREP_BUFFER_GET_DATA 78

struct remreq_buffer_set_data {
 
  struct message_header base;
  uint32_t winsys;
  uint32_t buffer;

};

#define REMREQ_BUFFER_SET_DATA 62

struct remrep_buffer_set_data {

  struct message_header base;
  boolean success;

};

#define REMREP_BUFFER_SET_DATA 79

struct remreq_create_screen {

  struct message_header base;

};

#define REMREQ_CREATE_SCREEN 64

struct remrep_create_screen {

  struct message_header base;
  uint32_t handle;

};

#define REMREP_CREATE_SCREEN 65

// winsys messages

struct remreq_flush_frontbuffer {

  struct message_header base;
  uint32_t screen;
  uint32_t surface;

};

#define REMREQ_FLUSH_FRONTBUFFER 66

struct remreq_buffer_create {

  struct message_header base;
  uint32_t screen;
  unsigned alignment, size, usage;
  uint32_t handle;

};

#define REMREQ_BUFFER_CREATE 67

struct remreq_user_buffer_create {

  struct message_header base;
  uint32_t screen;
  unsigned size;
  uint32_t handle;

};

#define REMREQ_USER_BUFFER_CREATE 68

struct remreq_user_buffer_update {

  struct message_header base;
  uint32_t screen;
  uint32_t buffer;

};

#define REMREQ_USER_BUFFER_UPDATE 69

/*
struct remreq_buffer_map {

  struct message_header base;
  uint32_t screen;
  uint32_t buffer;
  unsigned usage;
  boolean need_copy;

};

#define REMREQ_BUFFER_MAP 70

struct remrep_buffer_map {

  struct message_header base;
  boolean success;
  boolean has_data;
  
};

#define REMREP_BUFFER_MAP 77

struct remreq_buffer_unmap {

  struct message_header base;
  uint32_t screen;
  uint32_t buffer;
  boolean has_data;

};

#define REMREQ_BUFFER_UNMAP 71
*/

struct remreq_buffer_destroy {

  struct message_header base;
  uint32_t screen;
  uint32_t buffer;

};

#define REMREQ_BUFFER_DESTROY 72

struct remreq_swap_buffers {

  struct message_header base;
  uint32_t window;
  uint32_t texture;
 
};

#define REMREQ_SWAP_BUFFERS 76

#endif

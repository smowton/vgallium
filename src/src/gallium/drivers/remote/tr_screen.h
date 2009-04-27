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

#ifndef TR_SCREEN_H_
#define TR_SCREEN_H_

#include "pipe/p_screen.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PIPE_MAX_PARAMS 26
   
struct remote_screen
{

  struct pipe_screen base;
  uint32_t remote_handle;
  int socketfd;
  uint32_t tx_grants[4];
  uint32_t rx_grants[4];
  
  void* tx_buffer;
  void* rx_buffer;
  
  uint32_t last_query_handle;
  uint32_t last_dsa_handle;
  uint32_t last_rast_handle;  
  uint32_t last_blend_handle;
  uint32_t last_sampler_handle;
  uint32_t last_vs_handle;
  uint32_t last_fs_handle;
  uint32_t last_texture_handle;
  uint32_t last_surface_handle;
  uint32_t last_buffer_handle;
  
  uint32_t last_window_handle;

  int cached_int_params[PIPE_MAX_PARAMS];
  boolean int_param_is_cached[PIPE_MAX_PARAMS];
  float cached_float_params[PIPE_MAX_PARAMS];
  boolean float_param_is_cached[PIPE_MAX_PARAMS];

};


struct remote_screen *
remote_screen(struct pipe_screen *screen);

struct remote_winsys;

struct pipe_screen *
remote_screen_create(struct pipe_winsys *winsys);

int complete_screen_creation(struct remote_screen*);

uint32_t get_fresh_query_handle(struct pipe_screen*);
uint32_t get_fresh_blend_handle(struct pipe_screen*);
uint32_t get_fresh_rast_handle(struct pipe_screen*);
uint32_t get_fresh_dsa_handle(struct pipe_screen*);
uint32_t get_fresh_vs_handle(struct pipe_screen*);
uint32_t get_fresh_fs_handle(struct pipe_screen*);
uint32_t get_fresh_sampler_handle(struct pipe_screen*);
uint32_t get_fresh_buffer_handle(struct pipe_screen*);
uint32_t get_fresh_texture_handle(struct pipe_screen*);
uint32_t get_fresh_surface_handle(struct pipe_screen*);

#ifdef __cplusplus
}
#endif

#endif /* TR_SCREEN_H_ */

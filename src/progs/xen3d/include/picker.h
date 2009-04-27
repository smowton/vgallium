
#include "pipe/p_compiler.h"

#ifndef XEN3D_PICKER_H
#define XEN3D_PICKER_H

struct xen3d_control_message {

  uint32_t opcode;
  uint32_t length;

};

#define XEN3D_CONTROL_MESSAGE_CHOOSE_DOMAIN 0
#define XEN3D_CONTROL_MESSAGE_READY 1
#define XEN3D_CONTROL_MESSAGE_SET_CLIP 2
#define XEN3D_CONTROL_MESSAGE_SHOW 3

struct xen3d_clip_rect {

  int x;
  int y;
  int w;
  int h;

};

struct xen3d_control_message_choose_domain {

  struct xen3d_control_message base;
  uint32_t domain;

};

struct xen3d_control_message_set_clip {

  struct xen3d_control_message base;
  int offset_x;
  int offset_y;
  int nrects;

};

struct xen3d_control_message_show {

  struct xen3d_control_message base;
  int current_domain;
  int backdrop_width;
  int backdrop_height;

};

#endif

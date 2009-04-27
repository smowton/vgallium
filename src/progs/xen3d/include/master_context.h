
#ifndef XEN3D_MASTER_CONTEXT_H
#define XEN3D_MASTER_CONTEXT_H

#include "client.h"
#include "pipe/p_context.h"

struct pipe_context* get_master_context(struct global_state*);

void init_master_context(struct global_state*);

void draw_master_context(struct global_state*);

#endif

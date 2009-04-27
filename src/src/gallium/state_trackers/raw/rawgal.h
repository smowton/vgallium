
/* Exports for user programs */

#include "st_public.h"

struct pipe_context* get_current_pipe_context();

void* get_current_st_context();

struct pipe_surface* get_context_renderbuffer_surface(
	    struct st_context*, int index);
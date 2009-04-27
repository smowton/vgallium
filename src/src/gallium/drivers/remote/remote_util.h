
#include "remote_state.h"

#define PANIC_IF_NULL(ptr) if(!ptr) { printf("Unexpected null pointer; exiting.\n"); exit(1); }

#define SCREEN_HANDLE(screen) (screen ? ((struct remote_screen*)screen)->remote_handle : 0)

#define PIPE_HANDLE(pipe) (pipe ? ((struct remote_context*)pipe)->remote_handle : 0)

#define WINSYS_HANDLE(ws) (ws ? ((struct remote_winsys*)ws)->screen->remote_handle : 0)

#define SAMPLER_HANDLE(samp) (samp ? ((struct remote_opaque_sampler_state*)samp)->handle : 0)

#define QUERY_HANDLE(q) (q ? ((struct remote_pipe_query*)q)->handle : 0)

#define BLEND_HANDLE(b) (b ? ((struct remote_opaque_blend_state*)b)->handle : 0)

#define RAST_HANDLE(r) (r ? ((struct remote_opaque_rast_state*)r)->handle : 0)

#define DSA_HANDLE(dsa) (dsa ? ((struct remote_opaque_dsa_state*)dsa)->handle : 0)

#define FS_HANDLE(fs) (fs ? ((struct opaque_remote_fs*)fs)->handle : 0)

#define VS_HANDLE(vs) (vs ? ((struct opaque_remote_vs*)vs)->handle : 0)

#define TEXTURE_HANDLE(t) (t ? ((struct remote_texture*)t)->handle : 0)

#define SURFACE_HANDLE(s) (s ? ((struct remote_surface*)s)->handle : 0)

#define BUFFER_HANDLE(buf) (buf ? ((struct remote_buffer*)buf)->handle : 0)

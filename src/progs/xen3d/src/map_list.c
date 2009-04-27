
#include "client.h"
#include "map_list.h"

MAP_DECLARE_FUNCTIONS(uint32_t, struct client_context*, contextmap);
MAP_DECLARE_FUNCTIONS(uint32_t, struct client_texture*, texturemap);
MAP_DECLARE_FUNCTIONS(uint32_t, struct client_surface*, surfacemap);
MAP_DECLARE_FUNCTIONS(uint32_t, struct client_buffer*, buffermap);

MAP_DECLARE_DUMP(%u, %p, contextmap);
MAP_DECLARE_DUMP(%u, %p, texturemap);
MAP_DECLARE_DUMP(%u, %p, surfacemap);
MAP_DECLARE_DUMP(%u, %p, buffermap);

// Per-context maps

MAP_DECLARE_FUNCTIONS(uint32_t, struct client_blend*, blendmap);
MAP_DECLARE_FUNCTIONS(uint32_t, struct client_query*, querymap);
MAP_DECLARE_FUNCTIONS(uint32_t, struct client_sampler*, samplermap);
MAP_DECLARE_FUNCTIONS(uint32_t, struct client_rast*, rastmap);
MAP_DECLARE_FUNCTIONS(uint32_t, struct client_dsa*, dsamap);
MAP_DECLARE_FUNCTIONS(uint32_t, struct client_fs*, fsmap);
MAP_DECLARE_FUNCTIONS(uint32_t, struct client_vs*, vsmap);

MAP_DECLARE_DUMP(%u, %p, blendmap);
MAP_DECLARE_DUMP(%u, %p, querymap);
MAP_DECLARE_DUMP(%u, %p, samplermap);
MAP_DECLARE_DUMP(%u, %p, rastmap);
MAP_DECLARE_DUMP(%u, %p, dsamap);
MAP_DECLARE_DUMP(%u, %p, fsmap);
MAP_DECLARE_DUMP(%u, %p, vsmap);


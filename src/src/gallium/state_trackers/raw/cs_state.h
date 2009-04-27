
#include <pipe/p_state.h>

struct st_framebuffer;

struct st_context {

    /*struct cso_context* cso;*/
    
    struct st_framebuffer* draw_buffer;
    struct st_framebuffer* read_buffer;
    
    struct pipe_context* pipe;
    
    /*void* vs;
    void* fs;
    
    struct pipe_texture* default_texture;
    struct pipe_texture* sampler_textures[PIPE_MAX_SAMPLERS];*/
    
};

struct st_renderbuffer {

    struct pipe_surface* surface;
    struct pipe_texture* texture;
    enum pipe_format format;	
    
    struct st_framebuffer* framebuffer;
    
};

struct st_framebuffer {

    struct st_renderbuffer* buffer[9];
    
    unsigned int width, height;
    unsigned int samples;
    
    struct st_context* associated_context;
    
    unsigned int refcount;
    
    void* private_data;
    
    int needs_realloc;
    int needs_notify;
    
    int doubleBuffered;

};

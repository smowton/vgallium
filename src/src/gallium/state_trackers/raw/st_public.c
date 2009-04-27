
#include <cs_state.h>
#include <st_public.h>

#ifdef CS_DEBUG
#include <stdio.h>
#endif
#include <stdlib.h>

#include "GL/internal/glcore.h"  /* for __GLcontextModes */

#include "pipe/p_compiler.h"
#include "pipe/p_format.h"
#include <cso_cache/cso_context.h>
#include <util/u_simple_shaders.h>
#include <util/u_memory.h>
#include <pipe/p_context.h>
#include <pipe/p_state.h>
#include <pipe/p_inlines.h>
#include <pipe/p_shader_tokens.h>

#ifdef CS_DEBUG
#define DBG(format, args...) printf(format, ## args);
#else
#define DBG(format, args...)
#endif

/* Single, per-process "current" context for the time being */


struct st_context* global_current_context;

/* Our only upward-exports for programs using this */

void* get_current_st_context() {

    return global_current_context;
    
}

struct pipe_context* get_current_pipe_context() {

    if(global_current_context)
	return global_current_context->pipe;
    else
	return NULL;

}

struct pipe_surface* get_context_renderbuffer_surface(struct st_context* context, int index) {

    DBG("User requested framebuffer surface %d for context %p\n", index, (void*)context);

    if(context && context->draw_buffer && context->draw_buffer && context->draw_buffer->buffer[index])
	return context->draw_buffer->buffer[index]->surface;
    else {
	if(!context) {
	    DBG("Failed due to null context\n");
	}
	else if(!(context->draw_buffer)) {
	    DBG("Failed due to null draw_buffer\n");
	}
	else if(!(context->draw_buffer->buffer[index])) {
	    DBG("Failed due to null buffer[%d]\n", index);
	}
    }
	
}

/* End exports */

void deliver_framebuffer_notification(struct st_context*);

/* Function stolen from the Mesa state tracker, for want of not including or linking the entire thing;
   should probably move these into some state-tracker-shared area */
   
static int pf_is_depth_stencil( enum pipe_format format ) {
    return (pf_get_component_bits( format, PIPE_FORMAT_COMP_Z ) +
            pf_get_component_bits( format, PIPE_FORMAT_COMP_S )) != 0;
}

struct st_context *st_create_context(struct pipe_context *pipe,
                                     const __GLcontextModes *visual,
                                     struct st_context *share) {

   struct st_context *st_ctx;
   
   st_ctx = CALLOC_STRUCT(st_context);
   if(!st_ctx)
      return NULL;
      
    DBG("*** NEW CONTEXT: %p pertaining to pipe_context %p\n", (void*)st_ctx, (void*)pipe);

   st_ctx->pipe = pipe;

   /*st_ctx->cso = cso_create_context(st_ctx->pipe);
   if(!st_ctx->cso) {
      st_destroy_context(st_ctx);
      return NULL;
   }*/
   
   st_ctx->draw_buffer = 0;
   st_ctx->read_buffer = 0;

   global_current_context = st_ctx;
   
   DBG("Done creating new context\n");

   return st_ctx;

}
    

void st_destroy_context( struct st_context *st ) {

    DBG("Context destruction requested\n");

    /* Perhaps should be freeing our framebuffers... but the pipe might do this in response to the next call.
       Will find out... */

    /*cso_release_all(st->cso);
    
    cso_destroy_context(st->cso);*/
    
    st->pipe->destroy(st->pipe);
    
}


void st_copy_context_state(struct st_context *dst, struct st_context *src,  uint mask) {

    DBG("*** BUG: copy context not implemented\n");

    // Nothing for now. The Mesa implementation just passes this right to Mesa, which probably
    // calls back to us to set state in dst... therefore, need to find out what context is expected
    // to be copied on a GlXCopyContext call!
    // Alternatively if we decide to insist on EGL as the bottom-end interface then there is no copy.
    
}

int cs_allocate_renderbuffer(struct st_context* st, struct st_renderbuffer* strb) {

    struct pipe_context* pipe = st->pipe;
    
    struct pipe_texture template;
    unsigned surface_usage;
    
    DBG("(Re)allocating textures for renderbuffer %p (had texture %p, surface %p)\n", (void*)strb, (void*)strb->texture, (void*)strb->surface);
       
    /* Free the old surface and texture
     */
    pipe_surface_reference( &strb->surface, NULL );
    pipe_texture_reference( &strb->texture, NULL );
                    
    memset(&template, 0, sizeof(template));
                       
    /* init_renderbuffer_bits(strb, template.format);
     * This used to update GL's impression of the renderbuffer.
     * Might need to reintroduce something similar.
     */

    template.format = strb->format;
    template.target = PIPE_TEXTURE_2D;
    template.compressed = 0;
    pf_get_block(template.format, &template.block);
    template.width[0] = strb->framebuffer->width;
    template.height[0] = strb->framebuffer->height;
    template.depth[0] = 1;
    template.last_level = 0;
    template.nr_samples = strb->framebuffer->samples;

    if (pf_is_depth_stencil(template.format)) {
	DBG("New renderbuffer is a depth/stencil buffer\n");
	template.tex_usage = PIPE_TEXTURE_USAGE_DEPTH_STENCIL;
    }
    else {
	DBG("New renderbuffer is not a depth/stencil buffer\n");
	template.tex_usage = (PIPE_TEXTURE_USAGE_DISPLAY_TARGET |
	PIPE_TEXTURE_USAGE_RENDER_TARGET);
    }
    /* Probably need dedicated flags for surface usage too: 
    */
    surface_usage = (PIPE_BUFFER_USAGE_GPU_READ |
		    PIPE_BUFFER_USAGE_GPU_WRITE);
    
    strb->texture = pipe->screen->texture_create( pipe->screen, &template );
    
    DBG("Tried to allocate texture; got %p\n", (void*)strb->texture);

    /* Special path for accum buffers.  
     *
     * Try a different surface format.  Since accum buffers are s/w
     * only for now, the surface pixel format doesn't really matter,
     * only that the buffer is large enough.
     */
    if (!strb->texture && template.format == PIPE_FORMAT_R16G16B16A16_SNORM) 
    {
	/* Actually, just setting this usage value should be sufficient
	* to tell the driver to go ahead and allocate the buffer, even
	* if HW doesn't support the format.
	*/
	template.tex_usage = 0;
	surface_usage = (PIPE_BUFFER_USAGE_CPU_READ |
			PIPE_BUFFER_USAGE_CPU_WRITE);
	strb->texture = pipe->screen->texture_create( pipe->screen,
							&template );
							
	DBG("Tried accum-buffer alternative route, got %p\n", (void*)strb->texture);
    }
    if (!strb->texture) 
	return FALSE;
    strb->surface = pipe->screen->get_tex_surface(  pipe->screen,
						    strb->texture,
						    0, 0, 0,
						    surface_usage );
						
    DBG("Got a surface for new texture: %p\n", (void*)strb->surface);

    return strb->surface != NULL;

}

#define ST_SURFACE_STENCIL 7
#define ST_SURFACE_ACCUM 6

int enable_and_configure_buffer(struct st_renderbuffer** buffer, struct st_framebuffer* fb, enum pipe_format format) {

    DBG("Allocating a renderbuffer struct for framebuffer %p\n", (void*)fb);

    *buffer = CALLOC_STRUCT(st_renderbuffer);
    if(!(*buffer))
	return 0;

    DBG("Success; got new renderbuffer %p\n", (void*)*buffer);

    (*buffer)->framebuffer = fb;
    (*buffer)->format = format;
    (*buffer)->surface = NULL;
    (*buffer)->texture = NULL;
    
    return 1;
    
}    

struct st_framebuffer *st_create_framebuffer( const __GLcontextModes *visual,
                                              enum pipe_format colorFormat,
                                              enum pipe_format depthFormat,
                                              enum pipe_format stencilFormat,
                                              uint width, uint height,
                                              void *privateData) {
                                              
        DBG("***ENTERED ST_CREATE_FRAMEBUFFER\n");

	struct st_framebuffer* stfb = CALLOC_STRUCT(st_framebuffer);
	if(!stfb)
	    return (struct st_framebuffer*)0;
	
	DBG("Allocated new framebuffer struct: %p\n", (void*)stfb);
	    
	memset(stfb, 0, sizeof(struct st_framebuffer));
	
	stfb->refcount = 1;
	stfb->samples = 0;
	
	if(visual->sampleBuffers) 
	    stfb->samples = visual->samples;
	    
	stfb->width = width;
	stfb->height = height;
	stfb->private_data = privateData;
	stfb->needs_realloc = 1;
	stfb->needs_notify = 1;
	
	DBG("New buffer configured for %ux%u with %d samples\n", width, height, stfb->samples);
	    
	/* Front buffer */
	
	if(!enable_and_configure_buffer(&(stfb->buffer[ST_SURFACE_FRONT_LEFT]), stfb, colorFormat))
	    return (struct st_framebuffer*)0;
	
	DBG("Allocated a front-left buffer\n");
	
	if(visual->doubleBufferMode) {
	    if(!enable_and_configure_buffer(&(stfb->buffer[ST_SURFACE_BACK_LEFT]), stfb, colorFormat))
		return (struct st_framebuffer*)0;
	    stfb->doubleBuffered = 1;
	    DBG("Allocated a back-left buffer\n");
	}
	else {
	    stfb->doubleBuffered = 0;
	}
	    
	if(depthFormat == stencilFormat && depthFormat != PIPE_FORMAT_NONE) {
	    if(!enable_and_configure_buffer(&(stfb->buffer[ST_SURFACE_DEPTH]), stfb, depthFormat))
		return (struct st_framebuffer*)0;
	    stfb->buffer[ST_SURFACE_STENCIL] = stfb->buffer[ST_SURFACE_DEPTH];
	    DBG("Allocated a combined depth-stencil buffer\n");
	}
	else {
	    if(visual->depthBits > 0) {
		if(!enable_and_configure_buffer(&(stfb->buffer[ST_SURFACE_DEPTH]), stfb, depthFormat))
		    return (struct st_framebuffer*)0;
		DBG("Allocated a depth-only buffer\n");
	    }
	
		
	    if(visual->stencilBits > 0) {
		if(!enable_and_configure_buffer(&(stfb->buffer[ST_SURFACE_STENCIL]), stfb, stencilFormat))
		    return (struct st_framebuffer*)0;
		DBG("Allocated a stencil-only buffer\n");
	    }
	    if(visual->accumRedBits > 0) {
		if(!enable_and_configure_buffer(&(stfb->buffer[ST_SURFACE_ACCUM]), stfb, PIPE_FORMAT_R16G16B16A16_SNORM))
		    return (struct st_framebuffer*)0;
		DBG("Allocated an accumulation buffer\n");
	    }
	}
	
	DBG("***EXITING ST_CREATE_FRAMEBUFFER (RETURN %p)\n", (void*)stfb);
	
	return stfb;

}

void resize_framebuffer_now(struct st_framebuffer* stfb, uint width, uint height) {

    int i;
    int limit;

    DBG("(Re)allocating framebuffer %p to size %ux%u\n", (void*)stfb, width, height);
    
    if(stfb->buffer[ST_SURFACE_STENCIL] != NULL && stfb->buffer[ST_SURFACE_DEPTH] == stfb->buffer[ST_SURFACE_STENCIL]) {
	DBG("Reallocation special case: shared depth and stencil buffer\n");
	limit = ST_SURFACE_STENCIL;
    }
    else {
	limit = ST_SURFACE_DEPTH;
    }

    for(i = 0; i <= limit; i++) {
	if(stfb->buffer[i]) {
	
	    /* Problem here! According to Mesa's comments, resize can be called without a current
	       context. This would produce a call to gl_renderbuffer->AllocStorage with no current context,
	       which the Gallium drivers don't handle. 
	       
	       A brief look at other Mesa drivers (e.g. Nouveau) shows that they don't either. Either it 
	       never actually happens and the Mesa comments are out of date, or everybody is missing this
	       corner case.
	       
	       I'll attempt to catch it by tracking each framebuffer's most-recently-bound context, and checking
	       whether that is the current one. If it is not, I will simply store the new size and set a flag indicating
	       the buffer must be reallocated during makeCurrent.
	       
	       */
	       
	    DBG("Reconfiguring buffer %d\n", i);
	    cs_allocate_renderbuffer(stfb->associated_context, stfb->buffer[i]);
	}
    }
    
//    DBG("Informing the pipe of the new buffers\n");
//    deliver_framebuffer_notification(stfb->associated_context);
}

void fixup_framebuffer_now(struct st_framebuffer* stfb) {

    DBG("Entering framebuffer fixup for %p\n", (void*)stfb);

    if(stfb->needs_realloc) {
	DBG("Buffer needed (re)allocating\n");
	resize_framebuffer_now(stfb, stfb->width, stfb->height);
	stfb->needs_realloc = 0;
    }
    
    if(stfb->needs_notify) {
	DBG("Buffer needed notification\n");
	deliver_framebuffer_notification(stfb->associated_context);
	stfb->needs_notify = 0;
    }

}

void st_resize_framebuffer( struct st_framebuffer *stfb,
                            uint width, uint height ) {
                            
    /* Mesa checks for drivers which created a 0x0 framebuffer, and initializes them here.
       I'll assume that this doesn't happen for now. */
    
    DBG("***RESIZE: Framebuffer at %p to %ux%u (assoc-ctx: %p)\n", (void*)stfb, width, height, (void*)stfb->associated_context);

    if(stfb->width != width || stfb->height != height) {

        stfb->width = width;
	stfb->height = height;

	    /* Problem here! According to Mesa's comments, resize can be called without a current
	       context. This would produce a call to gl_renderbuffer->AllocStorage with no current context,
	       which the Gallium drivers don't handle. 
	       
	       A brief look at other Mesa drivers (e.g. Nouveau) shows that they don't either. Either it 
	       never actually happens and the Mesa comments are out of date, or everybody is missing this
	       corner case.
	       
	       I'll attempt to catch it by tracking each framebuffer's most-recently-bound context, and checking
	       whether that is the current one. If it is not, I will simply store the new size and set a flag indicating
	       the buffer must be reallocated during makeCurrent.
	       
	       */

	stfb->needs_realloc = 1;
	stfb->needs_notify = 1;

	if(stfb->associated_context != NULL && stfb->associated_context == global_current_context) {
    
	    DBG("Found framebuffer %p's associated context (%p) to be current; resizing now\n", (void*)stfb, (void*)stfb->associated_context);
	    fixup_framebuffer_now(stfb);
	
        }
	else {
	    DBG("Found framebuffer %p's associated context (%p) is null or not-current; deferring resize until next makeCurrent\n", (void*)stfb, (void*)stfb->associated_context);
	}

    }
    else {
	DBG("Resize: buffer already had size %ux%u; ignoring\n", width, height);
    }

}

void st_set_framebuffer_surface(struct st_framebuffer *stfb,
                                uint surfIndex, struct pipe_surface *surf) {
                                
    DBG("*** Asked to set surface %u of framebuffer %p to %p\n", surfIndex, (void*)stfb, (void*)surf);
                                
    if(stfb->buffer[surfIndex]) {
	if(stfb->buffer[surfIndex]->surface != surf) {
	    DBG("Named surface did exist, assigning surface %p and texture %p\n", (void*)surf, (void*)surf->texture);
	    pipe_surface_reference(&(stfb->buffer[surfIndex]->surface), surf);
	    pipe_texture_reference(&(stfb->buffer[surfIndex]->texture), surf->texture);
	    DBG("Old surface had dimensions %ux%u\n", stfb->width, stfb->height);
	    stfb->width = surf->texture->width[0];
	    stfb->height = surf->texture->height[0];
	    DBG("New surface has dimensions %ux%u\n", stfb->width, stfb->height);
	    stfb->needs_notify = 1;
	    if(stfb->associated_context && stfb->associated_context == global_current_context) {
		DBG("Found an associated context, %p, which was current, so notifying now\n", (void*)stfb->associated_context);
		fixup_framebuffer_now(stfb);
	    }
	}
	else {
	    DBG("Source and destination surfaces were equal; ignored\n");
	}
    }
    else {
	DBG("BUG (?) Asked to assign a surface which did not exist!\n");
    }
}

struct pipe_surface *st_get_framebuffer_surface(struct st_framebuffer *stfb, uint surfIndex) {

    if(stfb->buffer[surfIndex]) {
	DBG("Framebuffer surface for %p/%u requested, returning %p\n", (void*)stfb, surfIndex, (void*)stfb->buffer[surfIndex]->surface);
	return stfb->buffer[surfIndex]->surface;
    }
    else {
	DBG("*** FRAMEBUFFER SURFACE %p/%u did not exist\n", (void*)stfb, surfIndex);
	return (struct pipe_surface*)NULL;
    }
}

struct pipe_texture *st_get_framebuffer_texture(struct st_framebuffer *stfb, uint surfIndex) {

    if(stfb->buffer[surfIndex]) {
	DBG("Framebuffer texture for %p/%u requested, returning %p\n", (void*)stfb, surfIndex, (void*)stfb->buffer[surfIndex]->texture);
	return stfb->buffer[surfIndex]->texture;
    }
    else {
	DBG("*** FRAMEBUFFER TEXTURE %p/%u did not exist\n", (void*)stfb, surfIndex);
	return (struct pipe_texture*)NULL;
    }
}

void *st_framebuffer_private( struct st_framebuffer *stfb ) {

    return stfb->private_data;
    
}

void destroy_framebuffer(struct st_framebuffer* stfb) {

    int i;

    DBG("Destroying framebuffer %p\n", (void*)stfb);

    for(i = 0; i <= ST_SURFACE_DEPTH; i++) {
    
	if(stfb->buffer[i]) {

	    DBG("Destroying renderbuffer %d\n", i);
	    pipe_surface_reference(&(stfb->buffer[i]->surface), NULL);
	    pipe_texture_reference(&(stfb->buffer[i]->texture), NULL);
	    
	    free(stfb->buffer[i]);
	    
	}
	
    }
    
    free(stfb);
    
}

void st_unreference_framebuffer( struct st_framebuffer **stfb ) {

    
    if(*stfb) {
	DBG("Unreference framebuffer %p: new refcount %d\n", (void*)*stfb, (*stfb)->refcount);
	if(!(--((*stfb)->refcount))) {
	    destroy_framebuffer(*stfb);
	    *stfb = 0;
	}
    }
    else {
	DBG("Unref: null pointer\n");
    }
    
}

void deliver_framebuffer_notification(struct st_context* st) {

    
    int i;
    struct pipe_framebuffer_state* fbstate = CALLOC_STRUCT(pipe_framebuffer_state);
    
    memset(fbstate, 0, sizeof(struct pipe_framebuffer_state));

    fbstate->width = st->draw_buffer->width;
    fbstate->height = st->draw_buffer->height;

    fbstate->num_cbufs = 0;

    if(st->draw_buffer->doubleBuffered) {
	// Order back-left, front-left, back-right, front-right. Buffers come in pairs.
	DBG("Reporting buffers for double-buffered configuration\n");
	for(i = ST_SURFACE_FRONT_LEFT; i <= ST_SURFACE_BACK_LEFT; i+=2) {
	    if(st->draw_buffer->buffer[i]) {
		fbstate->cbufs[fbstate->num_cbufs++] = st->draw_buffer->buffer[i+1]->surface;
		fbstate->cbufs[fbstate->num_cbufs++] = st->draw_buffer->buffer[i]->surface;
	    }
	}
    }
    else {
	DBG("Reporting buffers for single-buffered configuration\n");
	for(i = ST_SURFACE_FRONT_LEFT; i <= ST_SURFACE_BACK_RIGHT; i++) {
	    if(st->draw_buffer->buffer[i])
		fbstate->cbufs[fbstate->num_cbufs++] = st->draw_buffer->buffer[i]->surface;
	}
    }

    if(st->draw_buffer->buffer[ST_SURFACE_DEPTH])
        fbstate->zsbuf = st->draw_buffer->buffer[ST_SURFACE_DEPTH]->surface;

    if(st->draw_buffer->buffer[ST_SURFACE_STENCIL])
        fbstate->zsbuf = st->draw_buffer->buffer[ST_SURFACE_STENCIL]->surface;
        
    DBG("Notifying pipe of new framebuffers for context %p\n", (void*)st);
    
    DBG("Colour buffers: %d\n", fbstate->num_cbufs);
    
    for(i = 0; i < fbstate->num_cbufs; i++) {
	DBG("Buffer %d: %p\n", i, (void*)fbstate->cbufs[i]);
    }
    
    if(fbstate->zsbuf) {
	DBG("Z/stencil buffer: %p\n", (void*)fbstate->zsbuf);
    }
    else {
	DBG("No Z/stencil buffer\n");
    }
    /* It seems the pipe cannot be informed of a seperate Z and stencil buffer, despite the
       fact that one could be created under create_framebuffer?  Also no mention can be made
       of the accumulation buffer, if one exists. Perhaps it's just a software abstraction? */

    st->pipe->set_framebuffer_state(st->pipe, fbstate);
    
}

void st_make_current(struct st_context *st,
                     struct st_framebuffer *draw,
                     struct st_framebuffer *read)
{

    /* In this simple state tracker, it's up to the user to make sure they only use the context which was
       most recently make_current'd; all rendering calls pass an explicit pointer to a context.
       
       Typically they would do this by examining the value of global_current_context.
       
       However, I do need to ensure that st's associated pipe is configured to use these buffers, and also
       ensure that the buffers have been allocated.
       
       Not sure at present that this does enough; _mesa_make_current does significantly more work, but I'm
       unsure which are required and which merely artefacts of OpenGL's conventions as to the state the system
       should be in after a MakeCurrent.
       
        */
        
        struct st_framebuffer* olddraw, *oldread;
        
        olddraw = st->draw_buffer;
        oldread = st->read_buffer;
        
        DBG("*** ENTERING ST_MAKE_CURRENT: Context %p to bind draw %p and read %p\n", (void*)st, (void*)draw, (void*)read);
        
        global_current_context = st;
        
	draw->associated_context = st;
	read->associated_context = st;
	
	st->draw_buffer = draw;
	st->read_buffer = read;
	
	if(olddraw != draw) {
	    DBG("Drawbuffer has changed (was %p, now %p)\n", (void*)olddraw, (void*)draw);
	    st_unreference_framebuffer(&olddraw);
	    draw->refcount++;
	    draw->needs_notify = 1;
	}
	if(oldread != read) {
	    DBG("Readbuffer has changed (was %p, now %p)\n", (void*)oldread, (void*)read);
	    read->needs_notify = 1;
	    read->refcount++;
	    st_unreference_framebuffer(&oldread);
	}
	
	if(draw->needs_realloc || draw->needs_notify) {
	    DBG("MakeCurrent found its draw-buffer (%p) needed realloc or notify; doing it now\n", (void*)draw);
	    fixup_framebuffer_now(draw);
	}
	if(read->needs_realloc || read->needs_notify) {
	    DBG("BUG! After draw didn't need resizing (or has been resized), found read still needs resizing.\n");
	    DBG("This means that draw != read. I need to find out how to tell the pipe about its read buffer, or whether\n");
	    DBG("Gallium implicitly assumes that the drawbuffer and readbuffer are the same.\n");
	    DBG("Draw was %p, read was %p\n", (void*)draw, (void*)read);
	}
	
	DBG("Leaving makeCurrent\n");
}

void st_flush( struct st_context *st, uint pipeFlushFlags,
               struct pipe_fence_handle **fence )
{

    DBG("Flush\n");

    st->pipe->flush(st->pipe, pipeFlushFlags, fence);
    
}

void st_finish( struct st_context *st ) {

    DBG("Finish\n");
    
    struct pipe_fence_handle* fence;

    st_flush(st, PIPE_FLUSH_RENDER_CACHE | PIPE_FLUSH_FRAME, &fence);
    
    st->pipe->winsys->fence_finish(st->pipe->winsys, fence, 0);
    st->pipe->winsys->fence_reference(st->pipe->winsys, &fence, NULL);
    
}

void st_notify_swapbuffers(struct st_framebuffer *stfb) {

    DBG("Swap buffers on %p\n", (void*)stfb);

    if(global_current_context->draw_buffer == stfb) {
    
	DBG("Current context used this buffer; flushing\n");
    
	st_flush( global_current_context, PIPE_FLUSH_RENDER_CACHE | PIPE_FLUSH_SWAPBUFFERS | PIPE_FLUSH_FRAME,
        	    NULL );

    }

}

void st_notify_swapbuffers_complete(struct st_framebuffer *stfb) {

    DBG("Swap buffers complete for %p\n", (void*)stfb);
    /* Stub for now; might not need to do anything here */
    
}

/* These two methods stubbed because:

    1. They aren't actually called from anywhere in the present codebase, and
    2. They're almost certainly only here for use with EGL's redirect-rendering-to-texture functions,
       which are better ignored in favour of GL's Frame Buffer Object extension or the equivalent direct
       pipe commands.
*/
    
/** Redirect rendering into stfb's surface to a texture image */
int st_bind_teximage(struct st_framebuffer *stfb, uint surfIndex,
                     int target, int format, int level) {

    DBG("BUG! Bind teximage not implemented\n");
    return 0;
    
}

/** Undo surface-to-texture binding */
int st_release_teximage(struct st_framebuffer *stfb, uint surfIndex,
                        int target, int format, int level) {
                        
    DBG("BUG! Release teximage not implemented\n");
    return 0;
    
}

st_proc st_get_proc_address(const char *procname) {

    DBG("BUG! Get_proc_address not implemented\n");
    return 0;

}

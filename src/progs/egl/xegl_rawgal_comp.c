/*
 * Copyright (C) 2008  Brian Paul   All Rights Reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * Draw a triangle with X/EGL and the raw Gallium interface
 * Brian Paul
 * 3 June 2008
 * Chris Smowton
 * 25th Sep 2008
 */


#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <EGL/egl.h>

#include <rawgal.h>

#include <pipe/p_state.h>
#include <pipe/p_context.h>
#include <pipe/p_screen.h>
#include <pipe/p_winsys.h>
#include <pipe/p_inlines.h>
#include <pipe/p_shader_tokens.h>
#include <util/u_simple_shaders.h>
#include <util/u_draw_quad.h>
#include <tgsi/tgsi_text.h>

struct pipe_context* getAsCurrent(EGLDisplay dpy, EGLSurface surf, EGLContext ctx) {

    if(!eglMakeCurrent(dpy, surf, surf, ctx)) {
	printf("Couldn't make-current the master!\n");
	exit(1);
    }
    
    return get_current_pipe_context();
    
}

void bind_state_atoms(struct pipe_context* context, struct pipe_blend_state* blend, struct pipe_depth_stencil_alpha_state* alpha,
			struct pipe_rasterizer_state* rasterizer, struct pipe_viewport_state* viewport,
			struct pipe_sampler_state* sampler, struct pipe_texture** textures, struct pipe_shader_state* vs,
			struct pipe_shader_state* fs) {
			
	if(blend) {
	    void* blendState = context->create_blend_state(context, blend);
	    context->bind_blend_state(context, blendState);
	}
	
	if(alpha) {
	    void* stencilState = context->create_depth_stencil_alpha_state(context, alpha);
	    context->bind_depth_stencil_alpha_state(context, stencilState);
	}
	
	if(rasterizer) {
	    void* rastState = context->create_rasterizer_state(context, rasterizer);
	    context->bind_rasterizer_state(context, rastState);
	}
	
	if(viewport) {
	    context->set_viewport_state(context, viewport);
	}
	
	if(sampler) {
	    void* samplers[PIPE_MAX_SAMPLERS];
	    
	    for(int i = 0; i < PIPE_MAX_SAMPLERS; i++) {
	      samplers[i] = context->create_sampler_state(context, sampler);
	    }
	    context->bind_sampler_states(context, PIPE_MAX_SAMPLERS, samplers);
	}
	
	if(textures) {
	    context->set_sampler_textures(context, PIPE_MAX_SAMPLERS, textures);
	}
	
	if(vs) {
	    void* shaderState = context->create_vs_state(context, vs);
	    context->bind_vs_state(context, shaderState);
	}
	
	if(fs) {
	    void* shaderState = context->create_fs_state(context, fs);
	    context->bind_fs_state(context, shaderState);
	}
	
}

void setFragmentShader(struct pipe_context* context, struct pipe_shader_state* fs) {

    bind_state_atoms(context, 0, 0, 0, 0, 0, 0, 0, fs);
    
}

void setFramebuffer(struct pipe_context* context, struct pipe_texture* buffer) {

    struct pipe_surface* target = context->screen->get_tex_surface( context->screen, 
                                          buffer, 0, 0, 0,
                                          PIPE_BUFFER_USAGE_GPU_WRITE | PIPE_BUFFER_USAGE_GPU_READ );
                                          
    if(!target) {
	printf("Couldn't get surface whilst setting framebuffer!\n");
	exit(1);
    }
    struct pipe_framebuffer_state newfbstate;

    memset(&newfbstate, 0, sizeof(struct pipe_framebuffer_state));
    
    newfbstate.width = buffer->width[0];
    newfbstate.height = buffer->height[0];
    newfbstate.num_cbufs = 1;
    newfbstate.cbufs[0] = target;
    
    context->set_framebuffer_state(context, &newfbstate);
    
}

void static_init_contexts(EGLDisplay dpy, EGLSurface surf, EGLContext masterCtx, EGLContext slave1, EGLContext slave2, struct pipe_texture* buffer1, struct pipe_texture* buffer2) {

    struct pipe_context* context;

    struct pipe_blend_state sharedBlend;
    
    memset(&sharedBlend, 0, sizeof(sharedBlend));
    sharedBlend.rgb_src_factor = PIPE_BLENDFACTOR_ONE;
    sharedBlend.alpha_src_factor = PIPE_BLENDFACTOR_ONE;
    sharedBlend.rgb_dst_factor = PIPE_BLENDFACTOR_ZERO;
    sharedBlend.alpha_dst_factor = PIPE_BLENDFACTOR_ZERO;
    sharedBlend.colormask = PIPE_MASK_RGBA;
    
    struct pipe_depth_stencil_alpha_state sharedDepthstencil;
    memset(&sharedDepthstencil, 0, sizeof(sharedDepthstencil));

    struct pipe_rasterizer_state sharedRasterizer;
    memset(&sharedRasterizer, 0, sizeof(sharedRasterizer));
    sharedRasterizer.front_winding = PIPE_WINDING_CW;
    sharedRasterizer.cull_mode = PIPE_WINDING_NONE;
    sharedRasterizer.bypass_clipping = 1;
    
    struct pipe_viewport_state sharedViewport;
    sharedViewport.scale[0] = 1.0;
    sharedViewport.scale[1] = 1.0;
    sharedViewport.scale[2] = 1.0;
    sharedViewport.scale[3] = 1.0;
    sharedViewport.translate[0] = 0.0;
    sharedViewport.translate[1] = 0.0;
    sharedViewport.translate[2] = 0.0;
    sharedViewport.translate[3] = 0.0;

    struct pipe_sampler_state sharedSampler;
    memset(&sharedSampler, 0, sizeof(sharedSampler));
    sharedSampler.wrap_s = PIPE_TEX_WRAP_CLAMP_TO_EDGE;
    sharedSampler.wrap_t = PIPE_TEX_WRAP_CLAMP_TO_EDGE;
    sharedSampler.wrap_r = PIPE_TEX_WRAP_CLAMP_TO_EDGE;
    sharedSampler.min_mip_filter = PIPE_TEX_MIPFILTER_NEAREST;
    sharedSampler.min_img_filter = PIPE_TEX_FILTER_NEAREST;
    sharedSampler.mag_img_filter = PIPE_TEX_FILTER_NEAREST;
    sharedSampler.normalized_coords = 1;

    /* Use the master context's screen to get textures.
       Presumably this means that for compositing to work, the contexts must share a screen */

    context = getAsCurrent(dpy, surf, masterCtx);

      /* default textures */
    struct pipe_screen *screen = context->screen;
    struct pipe_texture templat;
    struct pipe_surface *surface;
    unsigned i;
    struct pipe_texture* textures[PIPE_MAX_SAMPLERS];
    
    for(int i = 0; i < PIPE_MAX_SAMPLERS; i++)
	textures[i] = NULL;

    memset( &templat, 0, sizeof( templat ) );
    templat.target = PIPE_TEXTURE_2D;
    templat.format = PIPE_FORMAT_A8R8G8B8_UNORM;
    templat.block.size = 4;
    templat.block.width = 1;
    templat.block.height = 1;
    templat.width[0] = 1;
    templat.height[0] = 1;
    templat.depth[0] = 1;
    templat.last_level = 0;
    
    struct pipe_texture* defaultTexture = screen->texture_create( screen, &templat );
    if(defaultTexture) {
       surface = screen->get_tex_surface( screen, 
                                          defaultTexture, 0, 0, 0,
                                          PIPE_BUFFER_USAGE_CPU_WRITE );
       if(surface) {
          uint32_t *map;
          map = (uint32_t *) pipe_surface_map(surface, PIPE_BUFFER_USAGE_CPU_WRITE );
          if(map) {
             *map = 0x00000000;
             pipe_surface_unmap( surface );
          }
          pipe_surface_reference(&surface, NULL);
       }
    }
  
    for (i = 2; i < PIPE_MAX_SAMPLERS; i++)
      pipe_texture_reference(&(textures[i]), defaultTexture);
      
    // Textures zero and one will be the display targets of the slaves

    pipe_texture_reference(&(textures[0]), buffer1);
    pipe_texture_reference(&(textures[1]), buffer2);

 /* vertex shaders */

    struct pipe_shader_state colourNoop, textureNoop; /* Identical but for type informaton */
    colourNoop.tokens = malloc(sizeof(struct tgsi_token) * 1024);
    textureNoop.tokens = malloc(sizeof(struct tgsi_token) * 1024);

    if(!tgsi_text_translate("VERT1.1\nDCL IN[0], POSITION, CONSTANT\nDCL IN[1], GENERIC, CONSTANT\nDCL OUT[0], POSITION, CONSTANT\nDCL OUT[1], GENERIC, CONSTANT\n0:MOV OUT[0], IN[0]\n1:MOV OUT[1], IN[1]\n2:END", textureNoop.tokens, 1024)) {
	printf("Failed to parse texture vertex shader!\n");
	exit(1);
    }

    if(!tgsi_text_translate("VERT1.1\nDCL IN[0], POSITION, CONSTANT\nDCL IN[1], COLOR, CONSTANT\nDCL OUT[0], POSITION, CONSTANT\nDCL OUT[1], COLOR, CONSTANT\n0:MOV OUT[0], IN[0]\n1:MOV OUT[1], IN[1]\n2:END", colourNoop.tokens, 1024)) {
	printf("Failed to parse texture vertex shader!\n");
	exit(1);
    }

 /* fragment shaders */
    struct pipe_shader_state noopFragmentShader;
    noopFragmentShader.tokens = malloc(sizeof(struct tgsi_token) * 1024);

    if(!tgsi_text_translate("FRAG1.1\nDCL IN[0], COLOR, LINEAR\nDCL OUT[0], COLOR, CONSTANT\n0:MOV OUT[0], IN[0]\n1:END", noopFragmentShader.tokens, 1024)) {
	printf("Failed to parse noop fragment shader!\n");
	exit(1);
    }

/* ============================================== */
/* Done creating state objects; now to bind them to contexts */

    context = getAsCurrent(dpy, surf, masterCtx);
    bind_state_atoms(context, &sharedBlend, &sharedDepthstencil, &sharedRasterizer, &sharedViewport, &sharedSampler, textures,
			&textureNoop, NULL);
			
    context = getAsCurrent(dpy, surf, slave1);
    bind_state_atoms(context, &sharedBlend, &sharedDepthstencil, &sharedRasterizer, &sharedViewport, &sharedSampler, NULL,
			&colourNoop, &noopFragmentShader);

    context = getAsCurrent(dpy, surf, slave2);
    bind_state_atoms(context, &sharedBlend, &sharedDepthstencil, &sharedRasterizer, &sharedViewport, &sharedSampler, NULL,
			&colourNoop, &noopFragmentShader);

}

/* Big hack! The bug as dissected so far:

For some reason, compositing (i.e. one context drawing to a texture whilst
another reads from it) works fine for the top-left 64x64 chunk of the texture,
but either writes go missing or reads are redirected to a blank tile for the
rest of the texture.

For another reason, this doesn't apply to the first frame drawn.

The exact condition required to reset the pipe sufficiently that it draws
the full texture again is to change at least one of the textures bound to
the master context, i.e. that which is *reading* the textures.

The switched texture need not necessarily be the one actually being drawn;
for that reason, here I create two 1x1 textures to fill unoccupied texture
slots, and I strobe between the two on a frame-by-frame basis.

This suggests that the tile cache is assuming that the texture hasn't been
written in the meantime, but revises that assumption when any of the context's
bound textures change.

The odd bit is that this doesn't apply to the top-left tile.

I *think* that one context binding a texture whilst another writes to it is
legal, as EGL with OpenGLES seems to admit this situation. It's possible that
the Gallium guys haven't accomodated that particular API yet, as the state tracker
seems to include bind-buffer-to-texture commands which are stubbed.

*/

void static_init_contexts2(EGLDisplay dpy, EGLSurface surf, EGLContext masterCtx, EGLContext slave1, EGLContext slave2, struct pipe_texture* buffer1, struct pipe_texture* buffer2, struct pipe_texture* defaultTexture) {

    struct pipe_context* context;
    unsigned i;
    struct pipe_texture* textures[PIPE_MAX_SAMPLERS];
    
    for(int i = 0; i < PIPE_MAX_SAMPLERS; i++)
	textures[i] = NULL;

    for (i = 2; i < PIPE_MAX_SAMPLERS; i++)
	pipe_texture_reference(&(textures[i]), defaultTexture);
      
    // Textures zero and one will be the display targets of the slaves

    pipe_texture_reference(&(textures[0]), buffer1);
    pipe_texture_reference(&(textures[1]), buffer2);

    context = getAsCurrent(dpy, surf, masterCtx);
    bind_state_atoms(context, NULL, NULL, NULL, NULL, NULL, textures,
			NULL, NULL);

}

void pflush(struct pipe_context* context) {

     struct pipe_fence_handle* fence = NULL;
     context->flush(context, PIPE_FLUSH_RENDER_CACHE, &fence);
     context->winsys->fence_finish(context->winsys, fence, 0);
     context->winsys->fence_reference(context->winsys, &fence, NULL);
     
}

void drawGouraudTriangle(struct pipe_context* context, float left, float right) {

  struct pipe_buffer* vbuf;
  float* mappedBuffer;
 
  vbuf = pipe_buffer_create(context->screen, 32, PIPE_BUFFER_USAGE_VERTEX, sizeof(float) * 4 * 2 * 3);
 
  if(vbuf) {
   mappedBuffer = pipe_buffer_map(context->screen, vbuf, PIPE_BUFFER_USAGE_CPU_WRITE);
   if(mappedBuffer) {

     /* Format: XYZW RGBA */

     mappedBuffer[0] = left;
     mappedBuffer[1] = left;
     mappedBuffer[2] = 0.0;
     mappedBuffer[3] = 1.0;

     mappedBuffer[4] = 1.0;
     mappedBuffer[5] = 0.0;
     mappedBuffer[6] = 0.0;
     mappedBuffer[7] = 1.0;

     mappedBuffer[8] = left;
     mappedBuffer[9] = right;
     mappedBuffer[10] = 0.0;
     mappedBuffer[11] = 1.0;

     mappedBuffer[12] = 0.0;
     mappedBuffer[13] = 1.0;
     mappedBuffer[14] = 0.0;
     mappedBuffer[15] = 1.0;

     mappedBuffer[16] = right;
     mappedBuffer[17] = left;
     mappedBuffer[18] = 0.0;
     mappedBuffer[19] = 1.0;

     mappedBuffer[20] = 1.0;
     mappedBuffer[21] = 1.0;
     mappedBuffer[22] = 1.0;
     mappedBuffer[23] = 1.0;
     
     pipe_buffer_unmap(context->screen, vbuf);

     util_draw_vertex_buffer(context, vbuf, PIPE_PRIM_TRIANGLES, 3, 2);

     pipe_buffer_reference(context->screen, &vbuf, NULL);

   }
   else {
     printf("Couldn't map vertex buffer");
     exit(0);
   }
  }
  else {
    printf("Couldn't allocate vertex buffer");
    exit(0);
  }


}

void drawGouraudSquare(struct pipe_context* context, float left, float right) {

  struct pipe_buffer* vbuf;
  float* mappedBuffer;
 
  vbuf = pipe_buffer_create(context->screen, 32, PIPE_BUFFER_USAGE_VERTEX, sizeof(float) * 4 * 2 * 4);

  if(vbuf) {
   mappedBuffer = pipe_buffer_map(context->screen, vbuf, PIPE_BUFFER_USAGE_CPU_WRITE);
   if(mappedBuffer) {

     /* Format: XYZW RGBA */

     mappedBuffer[0] = left;
     mappedBuffer[1] = left;
     mappedBuffer[2] = 0.0;
     mappedBuffer[3] = 1.0;

     mappedBuffer[4] = 1.0;
     mappedBuffer[5] = 0.0;
     mappedBuffer[6] = 0.0;
     mappedBuffer[7] = 1.0;

     mappedBuffer[8] = left;
     mappedBuffer[9] = right;
     mappedBuffer[10] = 0.0;
     mappedBuffer[11] = 1.0;

     mappedBuffer[12] = 0.0;
     mappedBuffer[13] = 1.0;
     mappedBuffer[14] = 0.0;
     mappedBuffer[15] = 1.0;

     mappedBuffer[16] = right;
     mappedBuffer[17] = right;
     mappedBuffer[18] = 0.0;
     mappedBuffer[19] = 1.0;

     mappedBuffer[20] = 1.0;
     mappedBuffer[21] = 1.0;
     mappedBuffer[22] = 1.0;
     mappedBuffer[23] = 1.0;

     mappedBuffer[24] = right;
     mappedBuffer[25] = left;
     mappedBuffer[26] = 0.0;
     mappedBuffer[27] = 1.0;

     mappedBuffer[28] = 0.0;
     mappedBuffer[29] = 0.0;
     mappedBuffer[30] = 1.0;
     mappedBuffer[31] = 1.0;
     
     pipe_buffer_unmap(context->screen, vbuf);

     util_draw_vertex_buffer(context, vbuf, PIPE_PRIM_QUADS, 4, 2);

     pipe_buffer_reference(context->screen, &vbuf, NULL);
     
   }
   else {
     printf("Couldn't map vertex buffer");
     exit(0);
   }
  }
  else {
    printf("Couldn't allocate vertex buffer");
    exit(0);
  }


}

void drawTexturedSquare(struct pipe_context* context, float x, float y, float w, float h) {

  struct pipe_buffer* vbuf;
  float* mappedBuffer;
 
  vbuf = pipe_buffer_create(context->screen, 32, PIPE_BUFFER_USAGE_VERTEX, sizeof(float) * 4 * 2 * 4);
 
  if(vbuf) {
   mappedBuffer = pipe_buffer_map(context->screen, vbuf, PIPE_BUFFER_USAGE_CPU_WRITE);
   if(mappedBuffer) {

     /* Format: XYZW STQR */

     mappedBuffer[0] = x;
     mappedBuffer[1] = y;
     mappedBuffer[2] = 0.0;
     mappedBuffer[3] = 1.0;

     mappedBuffer[4] = 0.0;
     mappedBuffer[5] = 0.0;
     mappedBuffer[6] = 0.0;
     mappedBuffer[7] = 1.0;

     mappedBuffer[8] = x;
     mappedBuffer[9] = y + h;
     mappedBuffer[10] = 0.0;
     mappedBuffer[11] = 1.0;

     mappedBuffer[12] = 0.0;
     mappedBuffer[13] = 1.0;
     mappedBuffer[14] = 0.0;
     mappedBuffer[15] = 1.0;

     mappedBuffer[16] = x + w;
     mappedBuffer[17] = y + h;
     mappedBuffer[18] = 0.0;
     mappedBuffer[19] = 1.0;

     mappedBuffer[20] = 1.0;
     mappedBuffer[21] = 1.0;
     mappedBuffer[22] = 0.0;
     mappedBuffer[23] = 1.0;
     
     mappedBuffer[24] = x + w;
     mappedBuffer[25] = y;
     mappedBuffer[26] = 0.0;
     mappedBuffer[27] = 1.0;

     mappedBuffer[28] = 1.0;
     mappedBuffer[29] = 0.0;
     mappedBuffer[30] = 0.0;
     mappedBuffer[31] = 1.0;
     
     pipe_buffer_unmap(context->screen, vbuf);

     util_draw_vertex_buffer(context, vbuf, PIPE_PRIM_QUADS, 4, 2);

     pipe_buffer_reference(context->screen, &vbuf, NULL);

   }
   else {
     printf("Couldn't map vertex buffer");
     exit(0);
   }
  }
  else {
    printf("Couldn't allocate vertex buffer");
    exit(0);
  }


}


static void drawtri(EGLDisplay dpy, EGLSurface surf, EGLContext masterCtx, EGLContext slave1, EGLContext slave2) {

    /* First, create rendering target textures for the two slaves */

    struct pipe_context* context = getAsCurrent(dpy, surf, masterCtx);
    struct pipe_screen* screen = context->screen;
    struct pipe_surface* surface;
    
    struct pipe_texture templat;

    memset( &templat, 0, sizeof( templat ) );
    templat.target = PIPE_TEXTURE_2D;
    templat.format = PIPE_FORMAT_A8R8G8B8_UNORM;
    templat.block.size = 4;
    templat.block.width = 1;
    templat.block.height = 1;
    templat.width[0] = 1;
    templat.height[0] = 1;
    templat.depth[0] = 1;
    templat.last_level = 0;
    
    struct pipe_texture* defaultTexture = screen->texture_create( screen, &templat );
    if(defaultTexture) {
       surface = screen->get_tex_surface( screen, 
                                          defaultTexture, 0, 0, 0,
                                          PIPE_BUFFER_USAGE_CPU_WRITE );
       if(surface) {
          uint32_t *map;
          map = (uint32_t *) pipe_surface_map(surface, PIPE_BUFFER_USAGE_CPU_WRITE );
          if(map) {
             *map = 0x00000000;
             pipe_surface_unmap( surface );
          }
          pipe_surface_reference(&surface, NULL);
       }
    }

    struct pipe_texture* defaultTexture2 = screen->texture_create( screen, &templat );
    if(defaultTexture2) {
       surface = screen->get_tex_surface( screen, 
                                          defaultTexture2, 0, 0, 0,
                                          PIPE_BUFFER_USAGE_CPU_WRITE );
       if(surface) {
          uint32_t *map;
          map = (uint32_t *) pipe_surface_map(surface, PIPE_BUFFER_USAGE_CPU_WRITE );
          if(map) {
             *map = 0x00000000;
             pipe_surface_unmap( surface );
          }
          pipe_surface_reference(&surface, NULL);
       }
    }


    memset( &templat, 0, sizeof( templat ) );
    templat.target = PIPE_TEXTURE_2D;
    templat.format = PIPE_FORMAT_A8R8G8B8_UNORM;
    templat.block.size = 4;
    templat.block.width = 1;
    templat.block.height = 1;
    templat.width[0] = 1;
    templat.height[0] = 1;
    templat.depth[0] = 1;
    templat.last_level = 0;    
    templat.width[0] = 128;
    templat.height[0] = 128;
    templat.tex_usage = PIPE_TEXTURE_USAGE_SAMPLER | PIPE_TEXTURE_USAGE_DISPLAY_TARGET | PIPE_TEXTURE_USAGE_RENDER_TARGET;
    
    struct pipe_texture* target1 = screen->texture_create(screen, &templat);
    struct pipe_texture* target2 = screen->texture_create(screen, &templat);
    
    if(!target1 || !target2) {
	printf("Texture creation for targets failed!\n");
	exit(1);
    }

    /* Create fragment shaders which will paint the master's quads with these textures */

    struct pipe_shader_state tex0FragmentShader, tex1FragmentShader;
    
    tex0FragmentShader.tokens = malloc(sizeof(struct tgsi_token) * 1024);
    tex1FragmentShader.tokens = malloc(sizeof(struct tgsi_token) * 1024);

    if(!tgsi_text_translate("FRAG1.1\nDCL IN[0], GENERIC, LINEAR\nDCL OUT[0], COLOR, CONSTANT\nDCL SAMP[0], CONSTANT\n0:TEX OUT[0], IN[0], SAMP[0], 2D\n1:END", tex0FragmentShader.tokens, 1024)) {
	printf("Failed to parse tex0 fragment shader!\n");
	exit(1);
    }

    if(!tgsi_text_translate("FRAG1.1\nDCL IN[0], GENERIC, LINEAR\nDCL OUT[0], COLOR, CONSTANT\nDCL SAMP[1], CONSTANT\n0:TEX OUT[0], IN[0], SAMP[1], 2D\n1:END", tex1FragmentShader.tokens, 1024)) {
	printf("Failed to parse tex1 fragment shader!\n");
	exit(1);
    }

    /* Perform static init: set all state for the master and the slaves which will not change */
    
    static_init_contexts(dpy, surf, masterCtx, slave1, slave2, target1, target2);
    
    /* Now repeatedly draw for each context */

    for(int i = 0; i < 128; i++) {

	float x = (float)i;

	context = getAsCurrent(dpy, surf, slave1);
	setFramebuffer(context, target1);
	drawGouraudTriangle(context, 0.0, x);
	pflush(context);

	context = getAsCurrent(dpy, surf, slave2);
	setFramebuffer(context, target2);
	drawGouraudSquare(context, 0.0, x);
	pflush(context);

	context = getAsCurrent(dpy, surf, masterCtx);

	void* opaqueContext = get_current_st_context();
	struct pipe_surface* backBuffer = get_context_renderbuffer_surface(opaqueContext, 1);

	if(backBuffer)
	    context->clear(context, backBuffer, (unsigned int)0);
	else {
	    printf("Couldn't map the back buffer to clear!\n");
	    exit(1);
	}
	
	setFragmentShader(context, &tex0FragmentShader);
	drawTexturedSquare(context, x, 0, 150, 150);
	setFragmentShader(context, &tex1FragmentShader);
	drawTexturedSquare(context, 150 - x, 150, 150, 150);

	eglSwapBuffers(dpy, surf);
	if(i & 1)	
	    static_init_contexts2(dpy, surf, masterCtx, slave1, slave2, target1, target2, defaultTexture);
	else
	    static_init_contexts2(dpy, surf, masterCtx, slave1, slave2, target1, target2, defaultTexture2);

    }

}

/*
 * Create an RGB, double-buffered X window.
 * Return the window and context handles.
 */
static void
make_x_window(Display *x_dpy, EGLDisplay egl_dpy,
              const char *name,
              int x, int y, int width, int height,
              Window *winRet,
              EGLContext *ctxRet,
              EGLSurface *surfRet,
              EGLContext *ctxRet2,
              EGLContext *ctxRet3)
{
   static const EGLint attribs[] = {
      EGL_RED_SIZE, 1,
      EGL_GREEN_SIZE, 1,
      EGL_BLUE_SIZE, 1,
      EGL_DEPTH_SIZE, 1,
      EGL_NONE
   };

   int scrnum;
   XSetWindowAttributes attr;
   unsigned long mask;
   Window root;
   Window win;
   XVisualInfo *visInfo, visTemplate;
   int num_visuals;
   EGLContext ctx;
   EGLConfig config;
   EGLint num_configs;
   EGLint vid;

   scrnum = DefaultScreen( x_dpy );
   root = RootWindow( x_dpy, scrnum );

   if (!eglChooseConfig( egl_dpy, attribs, &config, 1, &num_configs)) {
      printf("Error: couldn't get an EGL visual config\n");
      exit(1);
   }

   assert(config);
   assert(num_configs > 0);

   if (!eglGetConfigAttrib(egl_dpy, config, EGL_NATIVE_VISUAL_ID, &vid)) {
      printf("Error: eglGetConfigAttrib() failed\n");
      exit(1);
   }

   /* The X window visual must match the EGL config */
   visTemplate.visualid = vid;
   visInfo = XGetVisualInfo(x_dpy, VisualIDMask, &visTemplate, &num_visuals);
   if (!visInfo) {
      printf("Error: couldn't get X visual\n");
      exit(1);
   }

   /* window attributes */
   attr.background_pixel = 0;
   attr.border_pixel = 0;
   attr.colormap = XCreateColormap( x_dpy, root, visInfo->visual, AllocNone);
   attr.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask;
   mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

   win = XCreateWindow( x_dpy, root, 0, 0, width, height,
		        0, visInfo->depth, InputOutput,
		        visInfo->visual, mask, &attr );

   /* set hints and properties */
   {
      XSizeHints sizehints;
      sizehints.x = x;
      sizehints.y = y;
      sizehints.width  = width;
      sizehints.height = height;
      sizehints.flags = USSize | USPosition;
      XSetNormalHints(x_dpy, win, &sizehints);
      XSetStandardProperties(x_dpy, win, name, name,
                              None, (char **)NULL, 0, &sizehints);
   }

   eglBindAPI(EGL_OPENGL_API);

   ctx = eglCreateContext(egl_dpy, config, EGL_NO_CONTEXT, NULL );
   if (!ctx) {
      printf("Error: eglCreateContext failed\n");
      exit(1);
   }

   *ctxRet2 = eglCreateContext(egl_dpy, config, EGL_NO_CONTEXT, NULL );
   if (!*ctxRet2) {
      printf("Error: eglCreateContext2 failed\n");
      exit(1);
   }

   *ctxRet3 = eglCreateContext(egl_dpy, config, EGL_NO_CONTEXT, NULL );
   if (!*ctxRet3) {
      printf("Error: eglCreateContext3 failed\n");
      exit(1);
   }

   *surfRet = eglCreateWindowSurface(egl_dpy, config, win, NULL);

   if (!*surfRet) {
      printf("Error: eglCreateWindowSurface failed\n");
      exit(1);
   }

   XFree(visInfo);

   *winRet = win;
   *ctxRet = ctx;

}

static void
usage(void)
{
   printf("Usage:\n");
   printf("  -display <displayname>  set the display to run on\n");
   printf("  -info                   display OpenGL renderer info\n");
}
 

int
main(int argc, char *argv[])
{
   const int winWidth = 300, winHeight = 300;
   Display *x_dpy;
   Window win;
   EGLSurface egl_surf;
   EGLContext egl_ctx, egl_ctx2, egl_ctx3;
   EGLDisplay egl_dpy;
   char *dpyName = NULL;
   int printInfo = 0;
   EGLint egl_major, egl_minor;
   int i;
   const char *s;

   for (i = 1; i < argc; i++) {
      if (strcmp(argv[i], "-display") == 0) {
         dpyName = argv[i+1];
         i++;
      }
      else if (strcmp(argv[i], "-info") == 0) {
         printInfo = 1;
      }
      else {
         usage();
         return -1;
      }
   }

   x_dpy = XOpenDisplay(dpyName);
   if (!x_dpy) {
      printf("Error: couldn't open display %s\n",
	     dpyName ? dpyName : getenv("DISPLAY"));
      return -1;
   }

   egl_dpy = eglGetDisplay(x_dpy);
   if (!egl_dpy) {
      printf("Error: eglGetDisplay() failed\n");
      return -1;
   }

   if (!eglInitialize(egl_dpy, &egl_major, &egl_minor)) {
      printf("Error: eglInitialize() failed\n");
      return -1;
   }

   s = eglQueryString(egl_dpy, EGL_VERSION);
   printf("EGL_VERSION = %s\n", s);

   make_x_window(x_dpy, egl_dpy,
                 "xegl_tri", 0, 0, winWidth, winHeight,
                 &win, &egl_ctx, &egl_surf, &egl_ctx2, &egl_ctx3);

   XMapWindow(x_dpy, win);

/*   if (printInfo) {
      printf("GL_RENDERER   = %s\n", (char *) glGetString(GL_RENDERER));
      printf("GL_VERSION    = %s\n", (char *) glGetString(GL_VERSION));
      printf("GL_VENDOR     = %s\n", (char *) glGetString(GL_VENDOR));
   }
*/
   
   drawtri(egl_dpy, egl_surf, egl_ctx, egl_ctx2, egl_ctx3);

   sleep(5);

   eglDestroyContext(egl_dpy, egl_ctx);
   eglDestroySurface(egl_dpy, egl_surf);
   eglTerminate(egl_dpy);


   XDestroyWindow(x_dpy, win);
   XCloseDisplay(x_dpy);

   return 0;
}

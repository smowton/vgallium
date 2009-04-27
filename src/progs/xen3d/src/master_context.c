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

#include <xorg/regionstr.h>
#include <xorg/miscstruct.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#include "client.h"

// The header for our extension to QEMU
#include "xen3d.h"

#ifdef CS_DEBUG_MASTER
#include <stdio.h>
#define DBG(format, args...) printf(format, ## args)
#else
#define DBG(format, args...)
#endif

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

// Useful code: draws ascii art to troubleshoot black screens!
/*
  printf("Main buffer: ---\n");

  struct pixel* map = (struct pixel*)pipe->screen->surface_map(pipe->screen, backBuffer, PIPE_BUFFER_USAGE_CPU_READ);
  
  for(int row = 0; row < backBuffer->height; row += 10) {
  for(int col = 0; col < backBuffer->width; col += 10) {
  if(map[(row * backBuffer->width) + col].green)
  printf(".%d", map[(row * backBuffer->width) + col].green);
  else
  printf("   ");
  }
  printf("\n");
  }
  
  printf("---\n");
*/

int shm_depth = 2;

struct pipe_context* getAsCurrent(EGLDisplay dpy, EGLSurface surf, EGLContext ctx) {

  if(!eglMakeCurrent(dpy, surf, surf, ctx)) {
    printf("Couldn't make-current the master!\n");
    exit(1);
  }
    
  return get_current_pipe_context();
    
}

struct pipe_context* get_master_context(struct global_state* state) {

  return getAsCurrent(state->egl_display, state->window_egl_surface, state->master_egl_context);

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

void init_master_context(struct global_state* state) {

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

  /* vertex shader */

  struct pipe_shader_state textureNoop, texture2dtex0;
  textureNoop.tokens = malloc(sizeof(struct tgsi_token) * 1024);

  if(!tgsi_text_translate("VERT1.1\nDCL IN[0], POSITION, CONSTANT\nDCL IN[1], GENERIC, CONSTANT\nDCL OUT[0], POSITION, CONSTANT\nDCL OUT[1], GENERIC, CONSTANT\n0:MOV OUT[0], IN[0]\n1:MOV OUT[1], IN[1]\n2:END", textureNoop.tokens, 1024)) {
    printf("Failed to parse texture vertex shader!\n");
    exit(1);
  }

  /* fragment shader */

  texture2dtex0.tokens = malloc(sizeof(struct tgsi_token) * 1024);

  if(!tgsi_text_translate("FRAG1.1\nDCL IN[0], GENERIC, LINEAR\nDCL OUT[0], COLOR, CONSTANT\nDCL SAMP[0], CONSTANT\n0:TEX OUT[0], IN[0], SAMP[0], 2D\n1:END", texture2dtex0.tokens, 1024)) {
    printf("Failed to parse tex0 fragment shader!\n");
    exit(1);
  }

  state->vs_tex_tokens = textureNoop.tokens;
  state->fs_tex_tokens = texture2dtex0.tokens;

  // Beware! This does not set the textures for the master context, but the fragment shader makes reference
  // to texture #0. This texture slot must be assigned before it is used.

  /* ============================================== */
  /* Done creating state objects; now to bind them to the master context */

  struct pipe_context* context = getAsCurrent(state->egl_display, state->window_egl_surface, state->master_egl_context);
  bind_state_atoms(context, &sharedBlend, &sharedDepthstencil, &sharedRasterizer, &sharedViewport, &sharedSampler, NULL,
		   &textureNoop, &texture2dtex0);

}

struct pixel {

  char red;
  char green;
  char blue;
  char alpha;

};

void draw_domain_with_clip_and_offset(struct global_state* state, 
				      struct domain_list_entry* domain, 
				      int offset_x, int offset_y, 
				      int nrects, struct xen3d_clip_rect* rects,
				      struct pipe_context* pipe, struct pipe_surface* backBuffer, 
				      int do_3d) {
  
  DBG("Drawing for domain %d, with offset (%d, %d)\n", domain->domain, offset_x, offset_y);
  DBG("Domain backdrop is clipped by %d rectangles\n", nrects);
  for(int i = 0; i < nrects; i++) {
    DBG("Rect %d: Origin (%d, %d) Dimension (%d, %d)\n", i, rects[i].x, rects[i].y, rects[i].w, rects[i].h);
  }
  
  // First, see if there is a backdrop for this domain, and if so draw it
  // The backdrop should be clipped by "rects" and drawn at offset (x, y).
  if(domain->qemu_shared_section) {
    
    if(domain->qemu_sem_handle != -1) {
      
      DBG("Found domain had a shared section and a semaphore object; drawing\n");
	
      if((!domain->backdrop_texture) ||
	 (domain->backdrop_width != domain->backdrop_texture->width[0]) ||
	 (domain->backdrop_height != domain->backdrop_texture->height[0])) {
	    
	DBG("Found 2D backdrop texture needed reallocation\n");

	// Need to realloc texture
		
	if(domain->backdrop_texture) {
	  
	  DBG("Found an existing texture (%p); freed\n", domain->backdrop_texture);
	  pipe->screen->texture_release(pipe->screen, &domain->backdrop_texture);
	    
	}
		    
	struct pipe_texture template;
	
	template.target = PIPE_TEXTURE_2D;
	template.format = state->window_pipe_format;	
	template.width[0] = domain->backdrop_width;
	template.height[0] = domain->backdrop_height;
	template.depth[0] = 1;
	template.last_level = 0;
	template.nr_samples = 1;
	template.compressed = 0;
	pf_get_block(template.format, &template.block);
	template.tex_usage = PIPE_TEXTURE_USAGE_SAMPLER;

	domain->backdrop_texture = pipe->screen->texture_create(pipe->screen, &template);
	  
	DBG("Allocated a new texture of dimension %dx%d; got %p\n", domain->backdrop_width, domain->backdrop_height, domain->backdrop_texture);
		
	if(!domain->backdrop_texture) {
	  printf("Failed to (re)allocate backdrop texture for domain %d (size %dx%d)\n", domain->domain, domain->backdrop_width, domain->backdrop_height);
	  domain = domain->next;
	  return;
	}
		
      }
	    
      // Now map the texture, grab the semaphore guarding the shared section,
      // and copy the new data in
	    
      // Perhaps for the future: to speed matters up, consider just drawing the
      // old version if the semaphore is currently taken? Also how about
      // having Qemu-dm discard updates if the compositor is already behind
      // to give it a chance to get up to date?

      struct pipe_surface* surf = pipe->screen->get_tex_surface(pipe->screen, domain->backdrop_texture, 0, 0, 0, PIPE_BUFFER_USAGE_CPU_WRITE);

      if(domain->dirty_w && domain->dirty_h) {
	
	DBG("Found backdrop for domain %d out of date\n", domain->domain);

	char* dest;
	if(surf) dest = (char*)pipe->screen->surface_map(pipe->screen, surf, PIPE_BUFFER_USAGE_CPU_WRITE);
	else dest = 0;
		
	if(!dest) {
	  printf("Failed to map backdrop texture for domain %d (size %dx%d)\n", domain->domain, domain->backdrop_width, domain->backdrop_height);
	  domain = domain->next;
	  if(surf) 
	    pipe->screen->tex_surface_release(pipe->screen, &surf);
	  return;
	}
		
	// Now do the copying

	char* src = (char*)domain->qemu_shared_section;
		
	int bytes_per_row = domain->dirty_w * (state->window_pixel_size / 8);
	int source_row_stride = domain->backdrop_width * (state->window_pixel_size / 8);
		
	int current_source_offset = (source_row_stride * domain->dirty_y) + (domain->dirty_x * (state->window_pixel_size / 8));
	int current_dest_offset = ((surf->stride * domain->dirty_y) + (domain->dirty_x * (state->window_pixel_size / 8))) + surf->offset;

	struct sembuf request;
	    
	request.sem_num = 0;
	request.sem_op = -1;
	request.sem_flg = 0;

	DBG("Getting lock for shared section\n");
	  
	int curval = semctl(domain->qemu_sem_handle, 0, GETVAL);
	  
	if(semop(domain->qemu_sem_handle, &request, 1) == -1) {
	  printf("Failed to get shared section lock drawing domain %d\n", domain->domain);
	  domain = domain->next;
	  pipe->screen->tex_surface_release(pipe->screen, &surf);
	  return;
	}
	  
	DBG("Got lock; copying in changed rect of origin (%d, %d) dimension (%d, %d)\n", domain->dirty_x, domain->dirty_y, domain->dirty_w, domain->dirty_h);

	for(int i = 0; i < domain->dirty_h; i++) {
	  memcpy(dest + current_dest_offset, src + current_source_offset, bytes_per_row);
	  current_source_offset += source_row_stride;
	  current_dest_offset += surf->stride;
	}		

	request.sem_op = 1;

	DBG("Releasing lock\n");

	if(semop(domain->qemu_sem_handle, &request, 1) == -1) {
	  printf("Failed to get shared section lock drawing domain %d\n", domain->domain);
	  domain = domain->next;
	  pipe->screen->tex_surface_release(pipe->screen, &surf);
	  return;
	}
	  
	domain->dirty_w = 0;
	domain->dirty_h = 0;
		
	pipe->screen->surface_unmap(pipe->screen, surf);
      }
	
      DBG("Drawing backdrop surface for domain %d\n", domain->domain);

      for(int i = 0; i < nrects; i++) {

	int dest_x = rects[i].x + offset_x;
	int dest_y = rects[i].y + offset_y;

	int width = MIN(domain->backdrop_width - rects[i].x, backBuffer->width - dest_x);
	width = MIN(rects[i].w, width);
	int height = MIN(domain->backdrop_height - rects[i].y, backBuffer->height - dest_y);
	height = MIN(rects[i].h, height);
	printf("Drawing to offset (%d, %d) with width (%d, %d)\n", dest_x, dest_y, width, height);

	pipe->surface_copy(pipe, 0, 
			   backBuffer, dest_x, dest_y, // Domain desktop coords
			   surf, rects[i].x, rects[i].y, 
			   width, height);

      }

      pipe->screen->tex_surface_release(pipe->screen, &surf);
	    
    }
  }
    
  if(!do_3d)
    return;
  // Done drawing the backdrop if there is one; now draw the client surfaces

  DBG("Drawing client surfaces for domain %d\n", domain->domain);

  struct drawable_surface_list* list = domain->drawables;

  while(list) {

    if(list->texture && list->ncliprects) {

      // i.e. if this drawable surface has been flipped at least once and the X extension
      // has reported on it at least once, and at least some of the surface is visible...
      
      struct pipe_texture* next_texture = list->texture->frontbuffer;

      //pipe->set_sampler_textures(pipe, 1, &next_texture);
      //drawTexturedSquare(pipe, 2 + (tileWidth * col), 2 + (tileHeight * row), drawWidth, drawHeight);

      struct pipe_surface* next_surface = pipe->screen->get_tex_surface(pipe->screen, next_texture, 0, 0, 0, PIPE_BUFFER_USAGE_GPU_READ | PIPE_BUFFER_USAGE_CPU_READ);

      // to do: offset all rendering according to the location of the X desktop itself on-screen.

      for(int i = 0; i < list->ncliprects; i++) {
	
	BoxPtr thisBox = &(list->cliprects[i]);
	    
	// Clipping is a bit complicated here, as the texture we have might be out of date compared to the
	// width and height demanded by the clipping rectangles, or vice versa. We need to draw the smallest
	// common rectangle of the requested rect, the source texture and the destination buffer.
	    
	int destx = thisBox->x1;
	if(destx > (int)backBuffer->width) {
	  DBG("Culled: off right side (box-x: %d, buffer-width: %u\n", destx, backBuffer->width);
	  continue;
	}
	    
	int desty = thisBox->y1;
	if(desty > (int)backBuffer->height) {
	  DBG("Culled: off bottom (box-y: %d, buffer-height: %u\n", desty, backBuffer->height);
	  continue;
	}
	    
	int srcx = thisBox->x1 - list->x;
	if(srcx > (int)next_surface->width) {
	  DBG("Culled: outside source X (box-x: %hd, window-x: %d, source width: %u\n", thisBox->x1, list->x, next_surface->width);
	  continue;
	}
	    
	int srcy = thisBox->y1 - list->y;
	if(srcy > (int)next_surface->height) {
	  DBG("Culled: outside source Y (box-y: %hd, window-y: %d, source height: %u\n", thisBox->y1, list->y, next_surface->height);
	  continue;
	}

	int width = thisBox->x2 - thisBox->x1;
	int height = thisBox->y2 - thisBox->y1;

	if(destx < 0) {
	  int rightshunt = -destx;
	  DBG("Clipped: left edge outside destination (was %d)\n", destx);
	  destx += rightshunt;
	  srcx += rightshunt;
	  width -= rightshunt;
	}
	    
	if(desty < 0) {
	  int downshunt = -desty;
	  DBG("Clipped: top edge outside destination (was %d)\n", desty);
	  desty += downshunt;
	  srcy += downshunt;
	  height -= downshunt;
	}

	// Looks as though it *is* necessary to clip against the top and left edges of the source, as the
	// X server's composition layer seems sometimes to be slightly out of sync with the main window object's
	// impression of its coordinates.

	if(srcx < 0) {
	  int rightshunt = -srcx;
	  DBG("Clipped: left edge outside source (was %d)\n", destx);
	  destx += rightshunt;
	  srcx += rightshunt;
	  width -= rightshunt;
	}
	    
	if(srcy < 0) {
	  int downshunt = -srcy;
	  DBG("Clipped: top edge outside source (was %d)\n", desty);
	  desty += downshunt;
	  srcy += downshunt;
	  height -= downshunt;
	}
	    	    
	if(srcx + width > (int)next_surface->width) {
	  DBG("Clipped: box width extends outside source (old width: %d, new width: %d, source X: %d, source width: %u\n", width, next_surface->width - srcx, srcx, next_surface->width);
	  width = next_surface->width - srcx;
	}
	if(destx + width > (int)backBuffer->width) {
	  DBG("Clipped: box width extends outside destination (old width: %d, new width: %d, dest X: %d, dest width: %u\n", width, backBuffer->width - destx, destx, backBuffer->width);
	  width = backBuffer->width - destx;
	}
	    
	if(srcy + height > (int)next_surface->height) {
	  DBG("Clipped: box height extends outside source (old height: %d, new height: %d, source Y: %d, source height: %u\n", height, next_surface->height - srcy, srcy, next_surface->height);
	  height = next_surface->height - srcy;
	}
	if(desty + height > (int)backBuffer->height) {
	  DBG("Clipped: box height extends outside destination (old height: %d, new height: %d, dest Y: %d, dest height: %u\n", height, backBuffer->height - desty, desty, backBuffer->height);
	  height = backBuffer->height - desty;
	}
	
	pipe->surface_copy(pipe, 0, 
			   backBuffer, destx, desty,
			   next_surface, srcx, srcy, 
			   width, height);
			   
      }

      pipe->screen->tex_surface_release(pipe->screen, &next_surface);

    }
    
    list = list->next;

  }

  DBG("Master: finished drawing\n");


}
  

void draw_master_context(struct global_state* state) {

  // Count the surfaces we should be drawing, decide therefore how big each should be, and finally do the draw.

  struct pipe_context* pipe = getAsCurrent(state->egl_display, state->window_egl_surface, state->master_egl_context);

  void* opaqueContext = get_current_st_context();
  struct pipe_surface* backBuffer = get_context_renderbuffer_surface(opaqueContext, 1);

  if(!backBuffer) {
    printf("Couldn't map the current back-buffer!\n");
    exit(1);
  }

  // Clear the back-buffer sillily by mapping and memsetting, to work around the bug with pipe->clear (see the ranty comment below).

  void* map = pipe->screen->surface_map(pipe->screen, backBuffer, PIPE_BUFFER_USAGE_CPU_WRITE);

  int surface_bytes = backBuffer->height * backBuffer->stride;
  
  memset(map, 0, surface_bytes);

  pipe->screen->surface_unmap(pipe->screen, backBuffer);

  // Clear current textures, to work around the softpipe tile cache's assumption that the texture is unchanged,
  // despite one of its surfaces having been copied into. 

  struct pipe_texture* no_tex = NULL;

  pipe->set_sampler_textures(pipe, 1, &no_tex);

  /* Sweet jumping jesus. For some reason, content drawn to a surface with copy_surface does not show up in surf->buffer,
     but does show up in the pointer yielded by surface_map, unless it has already been drawn over by the more ordinary rendering
     pipeline (e.g. by drawing a textured quad).

     This only comes up because softpipe's draw-textured-quad code is heck of slow; otherwise textured quads are the ideal way to do
     what I want to do in any case.

     So, for a rainy day: find out what Tungsten missed out from surface_copy (which for softpipe falls back to util_surface_copy)
     that does get done during ordinary drawing. I suspect something to do with softpipe's tile-cache, since generic fallbacks can't know about it.

     ARGH GOD DAMN IT. The eventual solution: pipe->clear, of all things, is the culprit. Do no clearing and everything works fine. This
     almost certainly indicates the softpipe's tile cache is broken and is feeding "cleared" tiles when they have actually been copied over.

     Since use of surface_copy is a path for util_surface_copy, this bug should surface for certain GL programs too.

  */

  DBG("Master context: ready to draw\n");

  if(state->current_domain) {
    struct xen3d_clip_rect rect;
    rect.x = 0;
    rect.y = 0;
    rect.w = state->current_domain->backdrop_width;
    rect.h = state->current_domain->backdrop_height;
    
    draw_domain_with_clip_and_offset(state, state->current_domain, 0, 0, 1, &rect, pipe, backBuffer, 1);
    /* Draw at (0,0) and do consider 3D clients */
  }

  if(state->control_dom_state == CONTROL_DOM_STATE_VISIBLE) {
    if(state->trusted_domain_descriptor) {
      draw_domain_with_clip_and_offset(state, state->trusted_domain_descriptor, 
				       state->control_dom_off_x, state->control_dom_off_y,
				       state->control_dom_n_clip_rects, state->control_dom_clip_rects,
				       pipe, backBuffer, 0);
      /* Draw at coordinates specified by the control domain, and don't consider 3D clients */
    }
  }

  eglSwapBuffers(state->egl_display, state->window_egl_surface);
  
}

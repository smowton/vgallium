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

static void drawtri(struct pipe_context* context, EGLDisplay dpy, EGLSurface surf) {

    struct pipe_blend_state blend;
    
    memset(&blend, 0, sizeof(blend));
    blend.rgb_src_factor = PIPE_BLENDFACTOR_ONE;
    blend.alpha_src_factor = PIPE_BLENDFACTOR_ONE;
    blend.rgb_dst_factor = PIPE_BLENDFACTOR_ZERO;
    blend.alpha_dst_factor = PIPE_BLENDFACTOR_ZERO;
    blend.colormask = PIPE_MASK_RGBA;
    
    void* blendState = context->create_blend_state(context, &blend);
    context->bind_blend_state(context, blendState);
    
    struct pipe_depth_stencil_alpha_state depthstencil;
    
    memset(&depthstencil, 0, sizeof(depthstencil));
    
    void* stencilState = context->create_depth_stencil_alpha_state(context, &depthstencil);
    context->bind_depth_stencil_alpha_state(context, stencilState);

    struct pipe_rasterizer_state rasterizer;
    
    memset(&rasterizer, 0, sizeof(rasterizer));
    rasterizer.front_winding = PIPE_WINDING_CW;
    rasterizer.cull_mode = PIPE_WINDING_NONE;
    rasterizer.bypass_clipping = 1;
    
    void* rastState = context->create_rasterizer_state(context, &rasterizer);
    context->bind_rasterizer_state(context, rastState);

    struct pipe_viewport_state viewport;
    viewport.scale[0] = 1.0;
    viewport.scale[1] = 1.0;
    viewport.scale[2] = 1.0;
    viewport.scale[3] = 1.0;
    viewport.translate[0] = 0.0;
    viewport.translate[1] = 0.0;
    viewport.translate[2] = 0.0;
    viewport.translate[3] = 0.0;

    context->set_viewport_state(context, &viewport);

    struct pipe_sampler_state sampler;
    memset(&sampler, 0, sizeof(sampler));
    sampler.wrap_s = PIPE_TEX_WRAP_CLAMP_TO_EDGE;
    sampler.wrap_t = PIPE_TEX_WRAP_CLAMP_TO_EDGE;
    sampler.wrap_r = PIPE_TEX_WRAP_CLAMP_TO_EDGE;
    sampler.min_mip_filter = PIPE_TEX_MIPFILTER_NONE;
    sampler.min_img_filter = PIPE_TEX_MIPFILTER_NEAREST;
    sampler.mag_img_filter = PIPE_TEX_MIPFILTER_NEAREST;
    sampler.normalized_coords = 1;

    void* samplers[PIPE_MAX_SAMPLERS];
    
    for(int i = 0; i < PIPE_MAX_SAMPLERS; i++) {
      samplers[i] = context->create_sampler_state(context, &sampler);
    }

    context->bind_sampler_states(context, PIPE_MAX_SAMPLERS, samplers);

      /* default textures */
   {
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
  
    for (i = 0; i < PIPE_MAX_SAMPLERS; i++)
      pipe_texture_reference(&(textures[i]), defaultTexture);
    
    context->set_sampler_textures(context, PIPE_MAX_SAMPLERS, textures);
 }
   
 /* vertex shader */

 {
    struct pipe_shader_state vert_shader;
    struct tgsi_token * tokens = malloc(sizeof(struct tgsi_token) * 1024);

    if(!tgsi_text_translate("VERT1.1\nDCL IN[0], POSITION, CONSTANT\nDCL IN[1], COLOR, CONSTANT\nDCL OUT[0], POSITION, CONSTANT\nDCL OUT[1], COLOR, CONSTANT\n0:MOV OUT[0], IN[0]\n1:MOV OUT[1], IN[1]\n2:END", tokens, 1024)) {
	printf("Failed to parse vertex shader!\n");
    }
    else {
      vert_shader.tokens = tokens;
      void* shaderState = context->create_vs_state(context, &vert_shader);
      context->bind_vs_state(context, shaderState);
    }
 }

 /* fragment shader */
 {
    struct pipe_shader_state frag_shader;
    struct tgsi_token* tokens = malloc(sizeof(struct tgsi_token) * 1024);

    if(!tgsi_text_translate("FRAG1.1\nDCL IN[0], COLOR, LINEAR\nDCL OUT[0], COLOR, CONSTANT\n0:MOV OUT[0], IN[0]\n1:END", tokens, 1024)) {
	printf("Failed to parse fragment shader!\n");
    }
    else {
      frag_shader.tokens = tokens;
      void* fragShaderState = context->create_fs_state(context, &frag_shader);
      context->bind_fs_state(context, fragShaderState);
    }
 } 

 /* And now, to draw the triangle... */

 for(int i = 90; i < 290; i++) {

  struct pipe_buffer* vbuf;
  float* mappedBuffer;
 
  vbuf = pipe_buffer_create(context->screen, 32, PIPE_BUFFER_USAGE_VERTEX, sizeof(float) * 4 * 2 * 3);
 
  printf("Submitting vertex buffer for drawing: %p\n", vbuf);

  if(vbuf) {
   mappedBuffer = pipe_buffer_map(context->screen, vbuf, PIPE_BUFFER_USAGE_CPU_WRITE);
   if(mappedBuffer) {

     /* Format: XYZW RGBA */

     mappedBuffer[0] = 10.0;
     mappedBuffer[1] = 10.0;
     mappedBuffer[2] = 0.0;
     mappedBuffer[3] = 1.0;

     mappedBuffer[4] = 1.0;
     mappedBuffer[5] = 1.0;
     mappedBuffer[6] = 1.0;
     mappedBuffer[7] = 1.0;

     mappedBuffer[8] = 10.0;
     mappedBuffer[9] = (float)i;
     mappedBuffer[10] = 0.0;
     mappedBuffer[11] = 1.0;

     mappedBuffer[12] = 1.0;
     mappedBuffer[13] = 0.0;
     mappedBuffer[14] = 0.0;
     mappedBuffer[15] = 1.0;

     mappedBuffer[16] = (float)i;
     mappedBuffer[17] = 10.0;
     mappedBuffer[18] = 0.0;
     mappedBuffer[19] = 1.0;

     mappedBuffer[20] = 0.0;
     mappedBuffer[21] = 1.0;
     mappedBuffer[22] = 0.0;
     mappedBuffer[23] = 1.0;

     pipe_buffer_unmap(context->screen, vbuf);

     util_draw_vertex_buffer(context, vbuf, PIPE_PRIM_TRIANGLES, 3, 2);

     pipe_buffer_reference(context->screen, &vbuf, NULL);
     
     eglSwapBuffers(dpy, surf);

   }
   else {
     printf("Couldn't map vertex buffer");
   }
  }
  else {
    printf("Couldn't allocate vertex buffer");
  }
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
              EGLSurface *surfRet)
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
      printf("Error: glXCreateContext failed\n");
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
   EGLContext egl_ctx;
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
                 &win, &egl_ctx, &egl_surf);

   XMapWindow(x_dpy, win);
   if (!eglMakeCurrent(egl_dpy, egl_surf, egl_surf, egl_ctx)) {
      printf("Error: eglMakeCurrent() failed\n");
      return -1;
   }

/*   if (printInfo) {
      printf("GL_RENDERER   = %s\n", (char *) glGetString(GL_RENDERER));
      printf("GL_VERSION    = %s\n", (char *) glGetString(GL_VERSION));
      printf("GL_VENDOR     = %s\n", (char *) glGetString(GL_VENDOR));
   }
*/
   
   struct pipe_context* masterContext = get_current_pipe_context();
   
   printf("Got a pipe context: %p\n", masterContext);
   
   drawtri(masterContext, egl_dpy, egl_surf);

   sleep(5);

   eglDestroyContext(egl_dpy, egl_ctx);
   eglDestroySurface(egl_dpy, egl_surf);
   eglTerminate(egl_dpy);


   XDestroyWindow(x_dpy, win);
   XCloseDisplay(x_dpy);

   return 0;
}

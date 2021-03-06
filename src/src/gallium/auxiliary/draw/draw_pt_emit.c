/**************************************************************************
 *
 * Copyright 2008 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

#include "util/u_memory.h"
#include "draw/draw_context.h"
#include "draw/draw_private.h"
#include "draw/draw_vbuf.h"
#include "draw/draw_vertex.h"
#include "draw/draw_pt.h"
#include "translate/translate.h"
#include "translate/translate_cache.h"

struct pt_emit {
   struct draw_context *draw;

   struct translate *translate;

   struct translate_cache *cache;
   unsigned prim;

   const struct vertex_info *vinfo;
};

void draw_pt_emit_prepare( struct pt_emit *emit,
			   unsigned prim,
                           unsigned *max_vertices )
{
   struct draw_context *draw = emit->draw;
   const struct vertex_info *vinfo;
   unsigned dst_offset;
   struct translate_key hw_key;
   unsigned i;
   boolean ok;
   
   /* XXX: need to flush to get prim_vbuf.c to release its allocation?? 
    */
   draw_do_flush( draw, DRAW_FLUSH_BACKEND );


   /* XXX: may need to defensively reset this later on as clipping can
    * clobber this state in the render backend.
    */
   emit->prim = prim;

   ok = draw->render->set_primitive(draw->render, emit->prim);
   if (!ok) {
      assert(0);
      return;
   }

   /* Must do this after set_primitive() above:
    */
   emit->vinfo = vinfo = draw->render->get_vertex_info(draw->render);


   /* Translate from pipeline vertices to hw vertices.
    */
   dst_offset = 0;
   for (i = 0; i < vinfo->num_attribs; i++) {
      unsigned emit_sz = 0;
      unsigned src_buffer = 0;
      unsigned output_format;
      unsigned src_offset = (vinfo->src_index[i] * 4 * sizeof(float) );


         
      switch (vinfo->emit[i]) {
      case EMIT_4F:
	 output_format = PIPE_FORMAT_R32G32B32A32_FLOAT;
	 emit_sz = 4 * sizeof(float);
	 break;
      case EMIT_3F:
	 output_format = PIPE_FORMAT_R32G32B32_FLOAT;
	 emit_sz = 3 * sizeof(float);
	 break;
      case EMIT_2F:
	 output_format = PIPE_FORMAT_R32G32_FLOAT;
	 emit_sz = 2 * sizeof(float);
	 break;
      case EMIT_1F:
	 output_format = PIPE_FORMAT_R32_FLOAT;
	 emit_sz = 1 * sizeof(float);
	 break;
      case EMIT_1F_PSIZE:
	 output_format = PIPE_FORMAT_R32_FLOAT;
	 emit_sz = 1 * sizeof(float);
	 src_buffer = 1;
	 src_offset = 0;
	 break;
      case EMIT_4UB:
	 output_format = PIPE_FORMAT_B8G8R8A8_UNORM;
	 emit_sz = 4 * sizeof(ubyte);
         break;
      default:
	 assert(0);
	 output_format = PIPE_FORMAT_NONE;
	 emit_sz = 0;
	 break;
      }
      
      hw_key.element[i].input_format = PIPE_FORMAT_R32G32B32A32_FLOAT;
      hw_key.element[i].input_buffer = src_buffer;
      hw_key.element[i].input_offset = src_offset;
      hw_key.element[i].output_format = output_format;
      hw_key.element[i].output_offset = dst_offset;

      dst_offset += emit_sz;
   }

   hw_key.nr_elements = vinfo->num_attribs;
   hw_key.output_stride = vinfo->size * 4;

   if (!emit->translate ||
       translate_key_compare(&emit->translate->key, &hw_key) != 0)
   {
      translate_key_sanitize(&hw_key);
      emit->translate = translate_cache_find(emit->cache, &hw_key);
   }

   *max_vertices = (draw->render->max_vertex_buffer_bytes / 
                    (vinfo->size * 4));

   /* even number */
   *max_vertices = *max_vertices & ~1;
}


void draw_pt_emit( struct pt_emit *emit,
		   const float (*vertex_data)[4],
		   unsigned vertex_count,
		   unsigned stride,
		   const ushort *elts,
		   unsigned count )
{
   struct draw_context *draw = emit->draw;
   struct translate *translate = emit->translate;
   struct vbuf_render *render = draw->render;
   void *hw_verts;

   /* XXX: need to flush to get prim_vbuf.c to release its allocation?? 
    */
   draw_do_flush( draw, DRAW_FLUSH_BACKEND );

   /* XXX: and work out some way to coordinate the render primitive
    * between vbuf.c and here...
    */
   if (!draw->render->set_primitive(draw->render, emit->prim)) {
      assert(0);
      return;
   }

   hw_verts = render->allocate_vertices(render,
					(ushort)translate->key.output_stride,
					(ushort)vertex_count);
   if (!hw_verts) {
      assert(0);
      return;
   }

   translate->set_buffer(translate, 
			 0, 
			 vertex_data,
			 stride );

   translate->set_buffer(translate, 
			 1, 
			 &draw->rasterizer->point_size,
			 0);

   translate->run( translate,
		   0, 
		   vertex_count,
		   hw_verts );

   render->draw(render,
		elts,
		count);

   render->release_vertices(render,
			    hw_verts,
			    translate->key.output_stride,
			    vertex_count);
}


void draw_pt_emit_linear(struct pt_emit *emit,
                         const float (*vertex_data)[4],
                         unsigned vertex_count,
                         unsigned stride,
                         unsigned start,
                         unsigned count)
{
   struct draw_context *draw = emit->draw;
   struct translate *translate = emit->translate;
   struct vbuf_render *render = draw->render;
   void *hw_verts;

#if 0
   debug_printf("Linear emit\n");
#endif
   /* XXX: need to flush to get prim_vbuf.c to release its allocation?? 
    */
   draw_do_flush( draw, DRAW_FLUSH_BACKEND );

   /* XXX: and work out some way to coordinate the render primitive
    * between vbuf.c and here...
    */
   if (!draw->render->set_primitive(draw->render, emit->prim)) {
      assert(0);
      return;
   }

   hw_verts = render->allocate_vertices(render,
					(ushort)translate->key.output_stride,
					(ushort)count);
   if (!hw_verts) {
      assert(0);
      return;
   }

   translate->set_buffer(translate, 0,
			 vertex_data, stride);

   translate->set_buffer(translate, 1,
			 &draw->rasterizer->point_size,
			 0);

   translate->run(translate,
                  0,
                  vertex_count,
                  hw_verts);

   if (0) {
      unsigned i;
      for (i = 0; i < vertex_count; i++) {
         debug_printf("\n\n%s vertex %d:\n", __FUNCTION__, i);
         draw_dump_emitted_vertex( emit->vinfo, 
                                   (const uint8_t *)hw_verts + 
                                   translate->key.output_stride * i );
      }
   }


   render->draw_arrays(render, start, count);

   render->release_vertices(render,
			    hw_verts,
			    translate->key.output_stride,
			    vertex_count);
}

struct pt_emit *draw_pt_emit_create( struct draw_context *draw )
{
   struct pt_emit *emit = CALLOC_STRUCT(pt_emit);
   if (!emit)
      return NULL;

   emit->draw = draw;
   emit->cache = translate_cache_create();
   if (!emit->cache) {
      FREE(emit);
      return NULL;
   }

   return emit;
}

void draw_pt_emit_destroy( struct pt_emit *emit )
{
   if (emit->cache)
      translate_cache_destroy(emit->cache);

   FREE(emit);
}

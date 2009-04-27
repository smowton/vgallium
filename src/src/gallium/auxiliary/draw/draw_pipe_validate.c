/**************************************************************************
 * 
 * Copyright 2007 Tungsten Graphics, Inc., Cedar Park, Texas.
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

/* Authors:  Keith Whitwell <keith@tungstengraphics.com>
 */

#include "util/u_memory.h"
#include "pipe/p_defines.h"
#include "draw_private.h"
#include "draw_pipe.h"
#include "draw_context.h"

static boolean points( unsigned prim )
{
   return (prim == PIPE_PRIM_POINTS);
}

static boolean lines( unsigned prim )
{
   return (prim == PIPE_PRIM_LINES ||
           prim == PIPE_PRIM_LINE_STRIP ||
           prim == PIPE_PRIM_LINE_LOOP);
}

static boolean triangles( unsigned prim )
{
   return prim >= PIPE_PRIM_TRIANGLES;
}

/**
 * Check if we need any special pipeline stages, or whether
 * prims/verts can go through untouched.  Don't test for bypass
 * clipping or vs modes, this function is just about the primitive
 * pipeline stages.
 */
boolean
draw_need_pipeline(const struct draw_context *draw,
                   const struct pipe_rasterizer_state *rasterizer,
                   unsigned int prim )
{
   /* Don't have to worry about triangles turning into lines/points
    * and triggering the pipeline, because we have to trigger the
    * pipeline *anyway* if unfilled mode is active.
    */
   if (lines(prim)) 
   {
      /* line stipple */
      if (rasterizer->line_stipple_enable && draw->pipeline.line_stipple)
         return TRUE;

      /* wide lines */
      if (rasterizer->line_width > draw->pipeline.wide_line_threshold)
         return TRUE;

      /* AA lines */
      if (rasterizer->line_smooth && draw->pipeline.aaline)
         return TRUE;
   }

   if (points(prim))
   {
      /* large points */
      if (rasterizer->point_size > draw->pipeline.wide_point_threshold)
         return TRUE;

      /* AA points */
      if (rasterizer->point_smooth && draw->pipeline.aapoint)
         return TRUE;

      /* point sprites */
      if (rasterizer->point_sprite && draw->pipeline.point_sprite)
         return TRUE;
   }


   if (triangles(prim)) 
   {
      /* polygon stipple */
      if (rasterizer->poly_stipple_enable && draw->pipeline.pstipple)
         return TRUE;

      /* unfilled polygons */
      if (rasterizer->fill_cw != PIPE_POLYGON_MODE_FILL ||
          rasterizer->fill_ccw != PIPE_POLYGON_MODE_FILL)
         return TRUE;
      
      /* polygon offset */
      if (rasterizer->offset_cw || rasterizer->offset_ccw)
         return TRUE;

      /* two-side lighting */
      if (rasterizer->light_twoside)
         return TRUE;
   }

   /* polygon cull - this is difficult - hardware can cull just fine
    * most of the time (though sometimes CULL_NEITHER is unsupported.
    * 
    * Generally this isn't a reason to require the pipeline, though.
    *
   if (rasterizer->cull_mode)
      return TRUE;
    */

   return FALSE;
}



/**
 * Rebuild the rendering pipeline.
 */
static struct draw_stage *validate_pipeline( struct draw_stage *stage )
{
   struct draw_context *draw = stage->draw;
   struct draw_stage *next = draw->pipeline.rasterize;
   int need_det = 0;
   int precalc_flat = 0;
   boolean wide_lines, wide_points;

   /* Set the validate's next stage to the rasterize stage, so that it
    * can be found later if needed for flushing.
    */
   stage->next = next;

   /* drawing wide lines? */
   wide_lines = (draw->rasterizer->line_width > draw->pipeline.wide_line_threshold
                 && !draw->rasterizer->line_smooth);

   /* drawing large points? */
   if (draw->rasterizer->point_sprite && draw->pipeline.point_sprite)
      wide_points = TRUE;
   else if (draw->rasterizer->point_smooth && draw->pipeline.aapoint)
      wide_points = FALSE;
   else if (draw->rasterizer->point_size > draw->pipeline.wide_point_threshold)
      wide_points = TRUE;
   else
      wide_points = FALSE;

   /*
    * NOTE: we build up the pipeline in end-to-start order.
    *
    * TODO: make the current primitive part of the state and build
    * shorter pipelines for lines & points.
    */

   if (draw->rasterizer->line_smooth && draw->pipeline.aaline) {
      draw->pipeline.aaline->next = next;
      next = draw->pipeline.aaline;
   }

   if (draw->rasterizer->point_smooth && draw->pipeline.aapoint) {
      draw->pipeline.aapoint->next = next;
      next = draw->pipeline.aapoint;
   }

   if (wide_lines) {
      draw->pipeline.wide_line->next = next;
      next = draw->pipeline.wide_line;
      precalc_flat = 1;
   }

   if (wide_points || draw->rasterizer->point_sprite) {
      draw->pipeline.wide_point->next = next;
      next = draw->pipeline.wide_point;
   }

   if (draw->rasterizer->line_stipple_enable && draw->pipeline.line_stipple) {
      draw->pipeline.stipple->next = next;
      next = draw->pipeline.stipple;
      precalc_flat = 1;		/* only needed for lines really */
   }

   if (draw->rasterizer->poly_stipple_enable
       && draw->pipeline.pstipple) {
      draw->pipeline.pstipple->next = next;
      next = draw->pipeline.pstipple;
   }

   if (draw->rasterizer->fill_cw != PIPE_POLYGON_MODE_FILL ||
       draw->rasterizer->fill_ccw != PIPE_POLYGON_MODE_FILL) {
      draw->pipeline.unfilled->next = next;
      next = draw->pipeline.unfilled;
      precalc_flat = 1;		/* only needed for triangles really */
      need_det = 1;
   }

   if (draw->rasterizer->flatshade && precalc_flat) {
      draw->pipeline.flatshade->next = next;
      next = draw->pipeline.flatshade;
   }
	 
   if (draw->rasterizer->offset_cw ||
       draw->rasterizer->offset_ccw) {
      draw->pipeline.offset->next = next;
      next = draw->pipeline.offset;
      need_det = 1;
   }

   if (draw->rasterizer->light_twoside) {
      draw->pipeline.twoside->next = next;
      next = draw->pipeline.twoside;
      need_det = 1;
   }

   /* Always run the cull stage as we calculate determinant there
    * also.  
    *
    * This can actually be a win as culling out the triangles can lead
    * to less work emitting vertices, smaller vertex buffers, etc.
    * It's difficult to say whether this will be true in general.
    */
   if (need_det || draw->rasterizer->cull_mode) {
      draw->pipeline.cull->next = next;
      next = draw->pipeline.cull;
   }

   /* Clip stage
    */
   if (!draw->bypass_clipping)
   {
      draw->pipeline.clip->next = next;
      next = draw->pipeline.clip;
   }

   
   draw->pipeline.first = next;
   return next;
}

static void validate_tri( struct draw_stage *stage, 
			  struct prim_header *header )
{
   struct draw_stage *pipeline = validate_pipeline( stage );
   pipeline->tri( pipeline, header );
}

static void validate_line( struct draw_stage *stage, 
			   struct prim_header *header )
{
   struct draw_stage *pipeline = validate_pipeline( stage );
   pipeline->line( pipeline, header );
}

static void validate_point( struct draw_stage *stage, 
			    struct prim_header *header )
{
   struct draw_stage *pipeline = validate_pipeline( stage );
   pipeline->point( pipeline, header );
}

static void validate_reset_stipple_counter( struct draw_stage *stage )
{
   struct draw_stage *pipeline = validate_pipeline( stage );
   pipeline->reset_stipple_counter( pipeline );
}

static void validate_flush( struct draw_stage *stage, 
			    unsigned flags )
{
   /* May need to pass a backend flush on to the rasterize stage.
    */
   if (stage->next)
      stage->next->flush( stage->next, flags );
}


static void validate_destroy( struct draw_stage *stage )
{
   FREE( stage );
}


/**
 * Create validate pipeline stage.
 */
struct draw_stage *draw_validate_stage( struct draw_context *draw )
{
   struct draw_stage *stage = CALLOC_STRUCT(draw_stage);
   if (stage == NULL)
      return NULL;

   stage->draw = draw;
   stage->next = NULL;
   stage->point = validate_point;
   stage->line = validate_line;
   stage->tri = validate_tri;
   stage->flush = validate_flush;
   stage->reset_stipple_counter = validate_reset_stipple_counter;
   stage->destroy = validate_destroy;

   return stage;
}

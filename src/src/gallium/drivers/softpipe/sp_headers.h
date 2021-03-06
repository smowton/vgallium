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

#ifndef SP_HEADERS_H
#define SP_HEADERS_H

#include "pipe/p_state.h"
#include "tgsi/tgsi_exec.h"

#define PRIM_POINT 1
#define PRIM_LINE  2
#define PRIM_TRI   3


/* The rasterizer generates 2x2 quads of fragment and feeds them to
 * the current fp_machine (see below).
 * Remember that Y=0=top with Y increasing down the window.
 */
#define QUAD_TOP_LEFT     0
#define QUAD_TOP_RIGHT    1
#define QUAD_BOTTOM_LEFT  2
#define QUAD_BOTTOM_RIGHT 3

#define MASK_TOP_LEFT     (1 << QUAD_TOP_LEFT)
#define MASK_TOP_RIGHT    (1 << QUAD_TOP_RIGHT)
#define MASK_BOTTOM_LEFT  (1 << QUAD_BOTTOM_LEFT)
#define MASK_BOTTOM_RIGHT (1 << QUAD_BOTTOM_RIGHT)
#define MASK_ALL          0xf


/**
 * Encodes everything we need to know about a 2x2 pixel block.  Uses
 * "Channel-Serial" or "SoA" layout.  
 */
struct quad_header_input
{
   int x0;
   int y0;
   float coverage[QUAD_SIZE];    /** fragment coverage for antialiasing */
   unsigned facing:1;   /**< Front (0) or back (1) facing? */
   unsigned prim:2;     /**< PRIM_POINT, LINE, TRI */
};

struct quad_header_inout
{
   unsigned mask:4;
};

struct quad_header_output
{
   /** colors in SOA format (rrrr, gggg, bbbb, aaaa) */
   float color[PIPE_MAX_COLOR_BUFS][NUM_CHANNELS][QUAD_SIZE];
   float depth[QUAD_SIZE];
};

struct quad_header {
   struct quad_header_input input;
   struct quad_header_inout inout;
   struct quad_header_output output;

   const struct tgsi_interp_coef *coef;
   const struct tgsi_interp_coef *posCoef;

   unsigned nr_attrs;
};

#endif /* SP_HEADERS_H */


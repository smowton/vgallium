/**************************************************************************
 *
 * Copyright 2006 Tungsten Graphics, Inc., Bismarck, ND., USA
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS, AUTHORS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 *
 **************************************************************************/
/*
 * Authors: Thomas Hellstr�m <thomas-at-tungstengraphics-dot-com>
 */

#ifndef _PSB_BUFPOOL_H_
#define _PSB_BUFPOOL_H_

#include <xf86drm.h>
#include "pipe/p_thread.h"
struct _DriFenceObject;

typedef struct _DriBufferPool
{
   int fd;
   int (*map) (struct _DriBufferPool * pool, void *private,
               unsigned flags, int hint, pipe_mutex *mutex,
	       void **virtual);
   int (*unmap) (struct _DriBufferPool * pool, void *private);
   int (*destroy) (struct _DriBufferPool * pool, void *private);
   unsigned long (*offset) (struct _DriBufferPool * pool, void *private);
   unsigned long (*poolOffset) (struct _DriBufferPool * pool, void *private);
   uint64_t (*flags) (struct _DriBufferPool * pool, void *private);
   unsigned long (*size) (struct _DriBufferPool * pool, void *private);
   void *(*create) (struct _DriBufferPool * pool, unsigned long size,
                    uint64_t flags, unsigned hint, unsigned alignment);
   void *(*reference) (struct _DriBufferPool * pool, unsigned handle);
   int (*unreference) (struct _DriBufferPool * pool, void *private);
   int (*fence) (struct _DriBufferPool * pool, void *private,
                 struct _DriFenceObject * fence);
   drmBO *(*kernel) (struct _DriBufferPool * pool, void *private);
   int (*validate) (struct _DriBufferPool * pool, void *private, pipe_mutex *mutex);
   int (*waitIdle) (struct _DriBufferPool *pool, void *private, pipe_mutex *mutex,
		    int lazy);
   int (*setStatus)  (struct _DriBufferPool *pool, void *private,
		      uint64_t flag_diff, uint64_t old_flags);
   void (*takeDown) (struct _DriBufferPool * pool);
   void *data;
} DriBufferPool;

extern void bmError(int val, const char *file, const char *function,
                    int line);
#define BM_CKFATAL(val)					       \
  do{							       \
    int tstVal = (val);					       \
    if (tstVal) 					       \
      bmError(tstVal, __FILE__, __FUNCTION__, __LINE__);       \
  } while(0);


/*
 * Builtin pools.
 */

/*
 * Kernel buffer objects. Size in multiples of page size. Page size aligned.
 */

extern struct _DriBufferPool *driDRMPoolInit(int fd);
extern struct _DriBufferPool *driMallocPoolInit(void);

struct _DriFreeSlabManager;
extern struct _DriBufferPool * driSlabPoolInit(int fd, uint64_t flags,
					       uint64_t validMask,
					       uint32_t smallestSize,
					       uint32_t numSizes,
					       uint32_t desiredNumBuffers,
					       uint32_t maxSlabSize,
					       uint32_t pageAlignment,
					       struct _DriFreeSlabManager *fMan);
extern void driFinishFreeSlabManager(struct _DriFreeSlabManager *fMan);
extern struct _DriFreeSlabManager *
driInitFreeSlabManager(uint32_t checkIntervalMsec, uint32_t slabTimeoutMsec);


#endif

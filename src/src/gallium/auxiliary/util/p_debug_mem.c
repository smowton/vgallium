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

/**
 * @file
 * Memory debugging.
 * 
 * @author José Fonseca <jrfonseca@tungstengraphics.com>
 */

#include "pipe/p_config.h" 

#if defined(PIPE_SUBSYSTEM_WINDOWS_DISPLAY)
#include <windows.h>
#include <winddi.h>
#elif defined(PIPE_SUBSYSTEM_WINDOWS_MINIPORT)
#include <wdm.h>
#else
#include <stdio.h>
#include <stdlib.h>
#endif

#include "pipe/p_debug.h" 
#include "util/u_double_list.h" 


#define DEBUG_MEMORY_MAGIC 0x6e34090aU 


#if defined(PIPE_SUBSYSTEM_WINDOWS_DISPLAY) && !defined(WINCE)
#define real_malloc(_size) EngAllocMem(0, _size, 'D3AG')
#define real_free(_ptr) EngFreeMem(_ptr)
#elif defined(PIPE_SUBSYSTEM_WINDOWS_MINIPORT)
#define real_malloc(_size) ExAllocatePool(0, _size)
#define real_free(_ptr) ExFreePool(_ptr)
#else
#define real_malloc(_size) malloc(_size)
#define real_free(_ptr) free(_ptr)
#endif


struct debug_memory_header 
{
   struct list_head head;
   
   unsigned long no;
   const char *file;
   unsigned line;
   const char *function;
   size_t size;
   unsigned magic;
};

struct debug_memory_footer
{
   unsigned magic;
};


static struct list_head list = { &list, &list };

static unsigned long last_no = 0;


static INLINE struct debug_memory_header *
header_from_data(void *data)
{
   if(data)
      return (struct debug_memory_header *)((char *)data - sizeof(struct debug_memory_header));
   else
      return NULL;
}

static INLINE void *
data_from_header(struct debug_memory_header *hdr)
{
   if(hdr)
      return (void *)((char *)hdr + sizeof(struct debug_memory_header));
   else
      return NULL;
}

static INLINE struct debug_memory_footer *
footer_from_header(struct debug_memory_header *hdr)
{
   if(hdr)
      return (struct debug_memory_footer *)((char *)hdr + sizeof(struct debug_memory_header) + hdr->size);
   else
      return NULL;
}


void *
debug_malloc(const char *file, unsigned line, const char *function,
             size_t size) 
{
   struct debug_memory_header *hdr;
   struct debug_memory_footer *ftr;
   
   hdr = real_malloc(sizeof(*hdr) + size + sizeof(*ftr));
   if(!hdr)
      return NULL;
 
   hdr->no = last_no++;
   hdr->file = file;
   hdr->line = line;
   hdr->function = function;
   hdr->size = size;
   hdr->magic = DEBUG_MEMORY_MAGIC;

   ftr = footer_from_header(hdr);
   ftr->magic = DEBUG_MEMORY_MAGIC;
   
   LIST_ADDTAIL(&hdr->head, &list);
   
   return data_from_header(hdr);
}

void
debug_free(const char *file, unsigned line, const char *function,
           void *ptr) 
{
   struct debug_memory_header *hdr;
   struct debug_memory_footer *ftr;
   
   if(!ptr)
      return;
   
   hdr = header_from_data(ptr);
   if(hdr->magic != DEBUG_MEMORY_MAGIC) {
      debug_printf("%s:%u:%s: freeing bad or corrupted memory %p\n",
                   file, line, function,
                   ptr);
      debug_assert(0);
      return;
   }

   ftr = footer_from_header(hdr);
   if(ftr->magic != DEBUG_MEMORY_MAGIC) {
      debug_printf("%s:%u:%s: buffer overflow %p\n",
                   hdr->file, hdr->line, hdr->function,
                   ptr);
      debug_assert(0);
   }

   LIST_DEL(&hdr->head);
   hdr->magic = 0;
   ftr->magic = 0;
   
   real_free(hdr);
}

void *
debug_calloc(const char *file, unsigned line, const char *function,
             size_t count, size_t size )
{
   void *ptr = debug_malloc( file, line, function, count * size );
   if( ptr )
      memset( ptr, 0, count * size );
   return ptr;
}

void *
debug_realloc(const char *file, unsigned line, const char *function,
              void *old_ptr, size_t old_size, size_t new_size )
{
   struct debug_memory_header *old_hdr, *new_hdr;
   struct debug_memory_footer *old_ftr, *new_ftr;
   void *new_ptr;
   
   if(!old_ptr)
      return debug_malloc( file, line, function, new_size );
   
   if(!new_size) {
      debug_free( file, line, function, old_ptr );
      return NULL;
   }
   
   old_hdr = header_from_data(old_ptr);
   if(old_hdr->magic != DEBUG_MEMORY_MAGIC) {
      debug_printf("%s:%u:%s: reallocating bad or corrupted memory %p\n",
                   file, line, function,
                   old_ptr);
      debug_assert(0);
      return NULL;
   }
   
   old_ftr = footer_from_header(old_hdr);
   if(old_ftr->magic != DEBUG_MEMORY_MAGIC) {
      debug_printf("%s:%u:%s: buffer overflow %p\n",
                   old_hdr->file, old_hdr->line, old_hdr->function,
                   old_ptr);
      debug_assert(0);
   }

   /* alloc new */
   new_hdr = real_malloc(sizeof(*new_hdr) + new_size + sizeof(*new_ftr));
   if(!new_hdr)
      return NULL;
   new_hdr->no = old_hdr->no;
   new_hdr->file = old_hdr->file;
   new_hdr->line = old_hdr->line;
   new_hdr->function = old_hdr->function;
   new_hdr->size = new_size;
   new_hdr->magic = DEBUG_MEMORY_MAGIC;
   
   new_ftr = footer_from_header(new_hdr);
   new_ftr->magic = DEBUG_MEMORY_MAGIC;
   
   LIST_REPLACE(&old_hdr->head, &new_hdr->head);

   /* copy data */
   new_ptr = data_from_header(new_hdr);
   memcpy( new_ptr, old_ptr, old_size < new_size ? old_size : new_size );

   /* free old */
   old_hdr->magic = 0;
   old_ftr->magic = 0;
   real_free(old_hdr);

   return new_ptr;
}

unsigned long
debug_memory_begin(void)
{
   return last_no;
}

void 
debug_memory_end(unsigned long start_no)
{
   size_t total_size = 0;
   struct list_head *entry;

   entry = list.prev;
   for (; entry != &list; entry = entry->prev) {
      struct debug_memory_header *hdr;
      void *ptr;
      hdr = LIST_ENTRY(struct debug_memory_header, entry, head);
      ptr = data_from_header(hdr);
      if((start_no <= hdr->no && hdr->no < last_no) ||
	 (last_no < start_no && (hdr->no < last_no || start_no <= hdr->no))) {
	 debug_printf("%s:%u:%s: %u bytes at %p not freed\n",
		      hdr->file, hdr->line, hdr->function,
		      hdr->size, ptr);
	 total_size += hdr->size;
      }
   }
   if(total_size) {
      debug_printf("Total of %u KB of system memory apparently leaked\n",
		   (total_size + 1023)/1024);
   }
}

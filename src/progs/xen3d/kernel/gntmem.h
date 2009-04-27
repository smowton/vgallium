/******************************************************************************
 * gntmem.h
 * 
 * Interface to /dev/gntmem.
 * 
 * Copyright (c) 2008, Chris Smowton
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation; or, when distributed
 * separately from the Linux kernel or incorporated into other
 * software packages, subject to the following license:
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this source file (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef __LINUX_PUBLIC_GNTMEM_H__
#define __LINUX_PUBLIC_GNTMEM_H__

/* Sets the domain to which memory will be granted. Must be issued before
   IOCTL_GNTMEM_SET_SIZE. Default: issue to domain 0. */
#define IOCTL_GNTMEM_SET_DOMAIN \
  _IOC(_IOC_WRITE, 'O', 0, sizeof(unsigned long))

/* Sets the size of the section of memory to be granted, in pages */
#define IOCTL_GNTMEM_SET_SIZE \
_IOC(_IOC_WRITE, 'O', 1, sizeof(unsigned long))       

/* Retrieves the grants applicable to the section pages, in order */
#define IOCTL_GNTMEM_GET_GRANTS \
  _IOC(_IOC_READ, 'O', 2, 0 /* Unknown size */)

#endif /* __LINUX_PUBLIC_GNTMEM_H__ */

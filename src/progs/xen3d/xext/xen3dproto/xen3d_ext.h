/*
* Copyright (c) 2006-2007 H. Andres Lagar-Cavilla <andreslc@cs.toronto.edu>
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*
*   1. Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
*   2. Redistributions in binary form must reproduce the above
* copyright notice, this list of conditions and the following disclaimer
* in the documentation and/or other materials provided with the
* distribution.
*   3. The name of the author may not be used to endorse or promote
* products derived from this software without specific prior written
* permission.
*
* THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
* IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
* INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
* HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
* STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
* IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/

/* vmgl project */

#ifndef _XEN3DEXT_H
#define _XEN3DEXT_H

/* Constants */
#define VMGL_EXTENSION_NAME	"Xen3DExt"
#define VMGL_MAJOR_VERSION		1
#define VMGL_MINOR_VERSION		0

#define X_VMGLWatchWindow		0

/* DOH! Turns out all X requests must be an integer number of DWORDS long! For reasons best understood to
   themselves they represent lengths by number-of-dwords in some places, but number-of-bytes in others. When
   one is converted to the other and then compared you get inconsistent things like 0 words, 2 bytes.
   This then cryptically explodes in XCBPutBuffer when it tries to process an unreasonably short partial. */

#define sz_xVMGLWatchWindowReq 16
typedef struct xVMGLWatchWindowReq_struct {
    CARD8       reqType;
    CARD8       VMGLRequestType;
    CARD16      length B16;
    CARD32      XWindow B32;
    CARD32	screenid B32;
    CARD32	glWindow B32;
} xVMGLWatchWindowReq;

#define sz_XVMGLWatchWindowReply 32
typedef struct XVMGLWatchWindowReply_struct {
    CARD8   	type;
    CARD8   	retVal;
    CARD16  	sequenceNumber B16;
    CARD32  	length B32;
    CARD32  	pad1 B32;
    CARD32  	pad2 B32;
    CARD32  	pad3 B32;
    CARD32  	pad4 B32;
    CARD32  	pad5 B32;
    CARD32  	pad6 B32;
} XVMGLWatchWindowReply;

#endif /* _XEN3DEXT_H */


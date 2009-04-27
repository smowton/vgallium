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

/* Xfree86 loadable module interface for the vmgl extension to the X server
 * vmgl project */
 
#ifdef HAVE_XORG_CONFIG_H
#include <xorg-config.h>
#endif

#include "xf86Module.h"

static MODULESETUPPROTO(Xen3DSetup);

extern void Xen3DExtensionInit(INITARGS);

static ExtensionModule VMGLExt = {
    Xen3DExtensionInit,
    "Xen3D Extension",
    NULL,
    NULL,
    NULL
};

static XF86ModuleVersionInfo VersRec = {
	"Xen3D Extension",
	"VMGL, H. Andres Lagar-Cavilla",
	MODINFOSTRING1,
	MODINFOSTRING2,
	XORG_VERSION_CURRENT,
	0, 1, 0,
	ABI_CLASS_EXTENSION,
	ABI_EXTENSION_VERSION,
	MOD_CLASS_EXTENSION,
	{0,0,0,0}
};

_X_EXPORT XF86ModuleData xen3dextModuleData = { &VersRec, Xen3DSetup, NULL };

static pointer
Xen3DSetup(pointer module, pointer opts, int *errmaj, int *errmin)
{
    LoadExtension(&VMGLExt, FALSE);

    /* Need a non-NULL return value to indicate success */
    return (pointer)1;
}


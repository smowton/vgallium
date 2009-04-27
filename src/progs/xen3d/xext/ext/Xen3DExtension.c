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

/* VMGL project: Body of the loadable extension for the Xorg Server */

#include <dix-config.h>
// CS: Added this so that _XSERVER64 gets defined where appropriate.
// this is important because XID needs to be 32 bits in length, and for some reason the X guys decided to go for #ifdef _XSERVER64 #define XID INT32 #else #define XID unsigned long #endif
// What was so wrong with plain old #define XID INT32 I don't know

#ifndef RedirectDrawNone
#define RedirectDrawNone 0
#endif

// Required to compile against at least Xorg 7.2, when RedirectDraw was a boolean
// Probably should figure out how to conditionally compile based on version (TODO)

#include "scrnintstr.h"
#include "extnsionst.h"
#include "dixstruct.h"
#include "windowstr.h"
#include "xf86Module.h"
#include "xf86.h"

#include "../../../composite/compint.h"

#include "regionstr.h"

#include <X11/extensions/xen3d_ext.h>
#include <X11/extensions/xen3d_extproto.h>

#include <xen/sys/gntmem.h>
#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
// For htonl and cousins
#include <unistd.h>
#include <fcntl.h>

/* List of windows to watch on behalf of a gl stub */
typedef struct GlWindowWatcher_struct {
    unsigned int XWindow;
    unsigned int glWindow;
    unsigned int screenid;
    struct GlWindowWatcher_struct *next;
} GlWindowWatcher;

/* Globals */
GlWindowWatcher *GlWindowWatchersList;
void (*ClipNotify_wrap[MAXSCREENS])();
Bool (*DestroyWindow_wrap[MAXSCREENS])();

int notifysock;
uint32_t* readoff;
uint32_t* writeoff;
uint32_t buffersize;
char* ringbuf;

/*extern WindowPtr *WindowTable;*/
// Since Xorg 7.1 this has been added to globals.h

int drain_events() {

  unsigned int flags = fcntl(notifysock, F_GETFL);
  
  unsigned int nbflags = flags | O_NONBLOCK;

  fcntl(notifysock, F_SETFL, nbflags);

  char waste[1024];

  int ret;

  while((ret = recv(notifysock, waste, 1024, 0)) > 0) { }

  fcntl(notifysock, F_SETFL, flags);

  if(ret == -1 && errno == EAGAIN)
    return 0;
  else {
    xf86ErrorF("Error draining events: return %d, errno %d\n", ret, errno);
    return -1;
  }

}

int wait_for_char(int fd, char c) {

  char received = c + 1;

  //  DBG("Waiting for a %c\n", c);

  while(received != c) {
    int ret = recv(fd, &received, 1, 0);
    //    DBG("Got a char: %c\n", received);
    if(ret <= 0) {
      xf86ErrorF("Error waiting for a %c (return %d, errno %d)\n", c, ret, errno);
      return -1;
    }
  }

  //  DBG("That's what I was looking for; returning success\n");

  return 0;

}


static int wait_for_tx_not_full() {

  //  DBG("Waiting for an event on the transmit ring\n");
  return wait_for_char(notifysock, 'T');

}

static int sendsome(char* data, int length) {

  uint32_t base = *writeoff;
  uint32_t limit = *readoff;
  uint32_t space;
  int writewrapsbuffer;

  if(limit > base) {
    space = (limit - base) - 1;
  }
  else {
    space = base - limit;
    space = (buffersize - space) - 1;
    // Care! > rather than >= because if it == buffersize, the write does
    // not wrap, but the pointer does 
  }

  int towrite = space >= length ? length : space;

  if((base + towrite) > buffersize)
    writewrapsbuffer = 1;
  else
    writewrapsbuffer = 0;

  if(towrite == 0) {
    // Looks like the buffer is full: let's wait      
    return 0;
  }

  // Okay, we have (towrite) bytes to write: let's do that.
           
  if(!writewrapsbuffer) {
    memcpy(ringbuf + base, data, towrite);
  }
  else {
    int bytestoend = buffersize - base;
    memcpy(ringbuf + base, data, bytestoend);
    memcpy(ringbuf, data + bytestoend, towrite - bytestoend);
  }

  return towrite;

}

static int blocking_send(void* data, int length) {

  int bytesSent = 0;

  while(bytesSent < length) {

    if(drain_events() == -1) {
      xf86ErrorF("Socket error draining events\n");
      return -1;
    }

    int sent = sendsome(((char*)data + bytesSent), length - bytesSent);

    if(sent == 0) {
      if(wait_for_tx_not_full() == -1) {
        xf86ErrorF("Socket error waiting for tx-not-full\n");
        return -1;
      }
    }

    uint32_t newwriteoff = *writeoff + sent;
    if(newwriteoff >= buffersize)
      newwriteoff -= buffersize;

    *writeoff = newwriteoff;

    bytesSent += sent;

    if(send(notifysock, "T", 1, 0) <= 0) {
      xf86ErrorF("Failed to send to the notify socket\n");
      return -1;
    }

  }    

  return length;

}

/* This function does the communication with a gl Stub */
static void SendWindowClipList(unsigned int screenid, unsigned int glWindow, RegionRec *clipList, int x, int y, unsigned int w, unsigned int h) {

    XVMGLWindowingCommand cmd;
    BoxPtr boxes;
    size_t writeLen;
    unsigned int len = REGION_NUM_RECTS(clipList);

    cmd.length = htonl(len);
    cmd.screenid = htonl(screenid);
    cmd.glWindow = htonl(glWindow);
    cmd.x = htonl(x);
    cmd.y = htonl(y);
    cmd.width = htonl(w);
    cmd.height = htonl(h);
    boxes = REGION_RECTS(clipList);
    
/*    xf86ErrorF("Command header as bytes: ");
    int i;
    for(i = 0; i < sizeof(XVMGLWindowingCommand); i++)
	xf86ErrorF("%hhu ", ((char*)&cmd)[i]);
	
    xf86ErrorF("\nSending %u cliprects\n", len);
*/
    writeLen = sizeof(XVMGLWindowingCommand);
    if (blocking_send((void *) &cmd, writeLen) != ((ssize_t) writeLen))
	xf86ErrorF("Error writing to ring\n");

    writeLen = sizeof(BoxRec)*len;
    
/*    xf86ErrorF("Boxes as bytes: ");
    for(i = 0; i < writeLen; i++)
	xf86ErrorF("%hhu ", ((char*)boxes)[i]);
    xf86ErrorF("\n");
*/    
    if (writeLen)
	if (blocking_send((void *) boxes, writeLen) != ((ssize_t) writeLen))
    	    xf86ErrorF("Error writing to ring\n");

#ifdef VERBOSE_VMGLEXT
    xf86ErrorF("SendWindowClipList for window %u (screen %u): %d %d %u %u -> %u\n",
	    glWindow, screenid, x, y, w, h, len);
#endif
}

/* Windowing functions hooks */
static GlWindowWatcher *findWindow(unsigned int XWindow) {
    GlWindowWatcher *ptr;

    for (ptr = GlWindowWatchersList; ptr->next != NULL; ptr = ptr->next)
        if (ptr->next->XWindow == XWindow)
            return ptr;

    return NULL;
}

static Bool VMGLDestroyWindow(WindowPtr pWin) {
    GlWindowWatcher *tmp, *ptr = findWindow(pWin->drawable.id);
    BOOL retVal = TRUE;
    
    if (ptr) {
	xf86Msg(X_INFO, "Destroy %u (screen %u) XID %u\n", ptr->next->glWindow, ptr->next->screenid, (unsigned int) pWin->drawable.id);
	tmp = ptr->next;
	ptr->next = tmp->next;
	xfree(tmp);
    }
    if (DestroyWindow_wrap[pWin->drawable.pScreen->myNum])
        retVal = (*DestroyWindow_wrap[pWin->drawable.pScreen->myNum])(pWin);
    /* Insist */
    screenInfo.screens[pWin->drawable.pScreen->myNum]->DestroyWindow = VMGLDestroyWindow;
    return retVal;
}

static void VMGLClipNotify(WindowPtr pWin, int x, int y) {
    GlWindowWatcher *ptr = findWindow(pWin->drawable.id);
    if (ptr) {
    
	RegionRec temp;
	RegionRec temp2;
	RegionRec* curTemp = &temp;
	RegionRec* nextTemp = &temp2;
	
	WindowPtr currentWin;
	
	REGION_NULL(pWin->drawable.pScreen, &temp);
	REGION_NULL(pWin->drawable.pScreen, &temp2);
	REGION_COPY(pWin->drawable.pScreen, &temp, &pWin->clipList);

	for(currentWin = pWin; currentWin; currentWin = currentWin->parent) {

	    if(currentWin->redirectDraw != RedirectDrawNone) {
		// Composition will be active for this window; need to retrieve its cliprects from the compositor
		CompWindowPtr cw = GetCompWindow(currentWin);
		if(!cw) {
		    ErrorF("Failed to get compositor window for regular window %p\n", pWin);
		    REGION_UNINIT(pWin->drawable.pScreen, &temp);
		    REGION_UNINIT(pWin->drawable.pScreen, &temp2);
		    return;
	        }
		ErrorF("Found composition\n");
	    	REGION_INTERSECT(currentWin->drawable.pScreen, nextTemp, curTemp, &cw->borderClip);
	    	RegionRec* x = curTemp;
	    	curTemp = nextTemp;
	    	nextTemp = x;
	    }

	}
	
	SendWindowClipList(ptr->next->screenid, ptr->next->glWindow, curTemp /*RegionRec*/,
    		pWin->drawable.x, pWin->drawable.y, pWin->drawable.width, pWin->drawable.height);
		
	REGION_UNINIT(pWin->drawable.pScreen, &temp);
	REGION_UNINIT(pWin->drawable.pScreen, &temp2);
    }
    if (ClipNotify_wrap[pWin->drawable.pScreen->myNum])
            (*ClipNotify_wrap[pWin->drawable.pScreen->myNum])(pWin, x, y);
    /* Insist */
    screenInfo.screens[pWin->drawable.pScreen->myNum]->ClipNotify = VMGLClipNotify;    
}

/* Ugly ugly ugly helper */
static WindowPtr DepthSearchHelper(unsigned int XWindow, WindowPtr root) {
    WindowPtr pWin, tmp;
    
    for (pWin = root->firstChild; pWin; pWin = pWin->nextSib) 
	if (pWin->drawable.id == XWindow) 
	    return pWin;
	else 
	    if ((tmp = DepthSearchHelper(XWindow, pWin)))
		return tmp;

    return NULL;
}

static WindowPtr DepthSearch(unsigned int XWindow) {
    int i;
    WindowPtr tmp;
    for (i=0;i<screenInfo.numScreens;i++)
	if ((tmp = DepthSearchHelper(XWindow, WindowTable[i])))
	    return tmp;
	    
    return NULL;
}

static Bool AddWindowWatch(unsigned int XWindow, unsigned int screenid, unsigned int glWindow)
{
    GlWindowWatcher *tmp, *ptr;
    WindowPtr thisWindow;
    
    if (!XWindow) {
	xf86ErrorF("Meaningless XID to watch\n");
	return FALSE;
    }
    
    /* Are we unwatching a window? */
    if (XWindow && !screenid && !glWindow) {
	for (ptr=GlWindowWatchersList; ptr->next != NULL; ptr=ptr->next) 
	    if (ptr->next->XWindow == XWindow) {
		xf86Msg(X_INFO, "Removing window %u\n", XWindow);
		tmp = ptr->next;
		ptr->next = tmp->next;
		xfree(tmp);
		return TRUE;
	    }
	/* Could not find it */
	return FALSE;
    }
    
    for (ptr=GlWindowWatchersList; ptr->next != NULL; ptr=ptr->next) 
	if (ptr->next->glWindow == glWindow && ptr->next->screenid == screenid) {
	    /* We have this, updating XWindow... */
	    ptr->next->XWindow = XWindow;
	    return TRUE;
	}
    
    /* We don't have this, add... */    
    tmp = (GlWindowWatcher *) xalloc(sizeof(GlWindowWatcher));
    if (!tmp) return FALSE;
    tmp->XWindow = XWindow;
    tmp->glWindow = glWindow;
    tmp->screenid = screenid;
    tmp->next = ptr->next;
    ptr->next = tmp;
    xf86ErrorF("Watching window %u on screen %u (XID %u)\n", glWindow, screenid, XWindow);
    if ((thisWindow = DepthSearch(XWindow))) {
	VMGLClipNotify(thisWindow, 0, 0); 
	xf86Msg(X_INFO, "ClipNotify sent for XID %u, screen %u, glWindow %u\n", XWindow, screenid, glWindow);
    } else xf86ErrorF("Odd, no window found for XID %u\n", XWindow);
    return TRUE;
}

/* These are X proto wrappers for the extension functions */

static int VMGLWatchWindowSrv(ClientPtr client)
{
    REQUEST(xVMGLWatchWindowReq);
    XVMGLWatchWindowReply reply;

    REQUEST_SIZE_MATCH(xVMGLWatchWindowReq);
    reply.retVal = AddWindowWatch(stuff->XWindow, stuff->screenid, stuff->glWindow) ? 1:0;
    
    reply.type = X_Reply;
    reply.length = 0;
    reply.sequenceNumber = client->sequence;
    if(client->swapped)
    {
	register char swapper;
    	swaps(&reply.sequenceNumber, swapper);
    }

    WriteToClient(client, SIZEOF(XVMGLWatchWindowReply), (char *) &reply);
    return (client->noClientException);
}

/* Dispatching. Boring. Need to handle swapping/endiannes as well */
static int NormalDispatcher(ClientPtr client)
{
    REQUEST(xReq);

    switch (stuff->data)
    {
	case X_VMGLWatchWindow:
	    return VMGLWatchWindowSrv(client);
	default:
	    return BadRequest;
    }
}

static int SwappedVMGLWatchWindowSrv(ClientPtr client)
{
    REQUEST(xVMGLWatchWindowReq);
    register char swapper;

    swaps(&stuff->length, swapper);
    REQUEST_SIZE_MATCH(xVMGLWatchWindowReq);
    swaps(&stuff->XWindow, swapper);
    swaps(&stuff->glWindow, swapper);
    swaps(&stuff->screenid, swapper);
    return VMGLWatchWindowSrv(client);
}

static int SwappedDispatcher(ClientPtr client)
{
    REQUEST(xReq);

    switch (stuff->data)
    {
	case X_VMGLWatchWindow:
	    return SwappedVMGLWatchWindowSrv(client);
	default:
	    return BadRequest;
    }
}

/* Extension initialization */
static void
VMGLDummyReset(ExtensionEntry *extEntry)
{
}

void Xen3DExtensionInit(INITARGS)
{
    ExtensionEntry *extEntry;
    int i;

    xf86ErrorF("Entered Xen3D Init...\n");

    /* Initialize extension */
    extEntry = AddExtension(VMGL_EXTENSION_NAME, 0, 0,
			    NormalDispatcher, SwappedDispatcher,
                            VMGLDummyReset, StandardMinorOpcode);
    if (!extEntry) {
	xf86ErrorF("Unable to start Xen3D extension\n");
	return;
    }

    xf86ErrorF("Added extension...\n");
        
    GlWindowWatchersList = (GlWindowWatcher *) xalloc(sizeof(GlWindowWatcher));
    GlWindowWatchersList->next = NULL;
    
    struct sockaddr_un addr;
    
    xf86ErrorF("Creating socket...\n");
    
    if ((notifysock = socket(PF_UNIX, SOCK_STREAM, 0)) < 0) {
	xf86ErrorF("Failed to create socket: %s\n", strerror(errno));
	return;
    }
    
    memset((void *) &addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, "/var/run/xen3dd-socket");
    
    xf86ErrorF("connecting...\n");
    
    if (connect(notifysock, (struct sockaddr *) &addr, sizeof(addr.sun_family) + strlen(addr.sun_path)) < 0) {
	xf86ErrorF("Failed to connect: %s\n", strerror(errno));
	return;
    }
    
    xf86ErrorF("Successfully connected to xen3dd\n");
    
    for (i = 0; i < screenInfo.numScreens; i++)
    {
        ClipNotify_wrap[i] = screenInfo.screens[i]->ClipNotify;
        screenInfo.screens[i]->ClipNotify = VMGLClipNotify;
        DestroyWindow_wrap[i] = screenInfo.screens[i]->DestroyWindow;
        screenInfo.screens[i]->DestroyWindow = VMGLDestroyWindow;
    }

    int fd = open("/dev/gntmem", O_RDWR);
    int ret;

    //    DBG("Opened /dev/gntmem; got fd %d\n", fd);

    if(fd == -1) {
      xf86ErrorF("Failed to open /dev/gntmem\n");
      return;
    }

    ret = ioctl(fd, IOCTL_GNTMEM_SET_DOMAIN, 0);

    //    DBG("set-domain 0 returned %d\n", ret);

    if(ret == -1) {
      xf86ErrorF("Failed to set gntmem's target domain\n");
      return;
    }

    ret = ioctl(fd, IOCTL_GNTMEM_SET_SIZE, 4);

    //    DBG("set-size 4 returned %d\n", ret);

    if(ret == -1) {
      xf86ErrorF("Failed to create a shareable section of 4 pages\n");
      return;
    }

    //    DBG("Going to write grants starting at address %p\n", togrant[i].grants);

    uint32_t grants[4];

    ret = ioctl(fd, IOCTL_GNTMEM_GET_GRANTS, grants);

    char* map = (char*)mmap(0, 4096 * 4, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if(map == (char*)-1) {
      xf86ErrorF("Failed to map my ring buffer\n");
      return;
    }

    close(fd);
    // Mapping will persist until the extension is unloaded

    uint32_t* initdata = (uint32_t*)map;
    buffersize = (4096 * 4) - (sizeof(uint32_t) * 3);
    initdata[0] = buffersize;
    initdata[1] = 0;
    initdata[2] = 0;
    // Read and write pointers coincide: ring is empty

    readoff = &(initdata[1]);
    writeoff = &(initdata[2]);
    ringbuf = (char*)&(initdata[3]);

    // Setup complete, now to notify xen3dd so it can create a xenstore entry

    struct {
      uint32_t rx_grants[4];
      uint32_t tx_grants[4];
      char is_x;
    } tosend;

    for(i = 0; i < 4; i++) {
      tosend.tx_grants[i] = grants[i];
      tosend.rx_grants[i] = 0;
      // The X extension does not receive
    }

    tosend.is_x = 1;

    int bytessent = 0;

    while(bytessent < sizeof(tosend)) {

      int sent = send(notifysock, ((char*)&tosend) + bytessent, sizeof(tosend) - bytessent, 0);

      if(sent <= 0) {
	xf86ErrorF("Failed to send init message to xen3dd: return %d with errno %d\n", sent, errno);
	return;
      }
      else {
	bytessent += sent;
      }

    }

    // Setup complete!

    xf86Msg(X_INFO, "Xen3D extension succesfully loaded\n");

    return;
}

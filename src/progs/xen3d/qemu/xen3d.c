/*
 * QEMU Xen3d display driver
 * 
 * By Chris Smowton
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include "qemu-common.h"
#include "console.h"
#include "sysemu.h"

#include <stdio.h>
#include <Hermes/Hermes.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "xen3d.h"

HermesFormat* source_format = 0;
HermesFormat* shm_format = 0;
int shm_depth = 0;
int shm_pixel_bytes = 0;
void* shmtarget = 0;
int sockfd = 0;
int compositor_semaphore = -1;
HermesHandle hermes_handle = 0;
HermesHandle hermes_palette = 0;
void* my_buffer = 0;

int screen_width = 2;
int screen_height = 2;

int mouse_state = 0;
int mouse_last_x = 0;
int mouse_last_y = 0;

char* input_command;
char* input_current;
char* input_target;

struct pixel {

    char r;
    char g;
    char b;
    char a;

};

static void xen3d_die(DisplayState*);

static int recv_all(int fd, char* message, int size) {

    int bytessent = 0;
    int sent;

    while(bytessent < size) {
	if((sent = recv(fd, message + bytessent, size - bytessent, 0)) <= 0) {
	    fprintf(stderr, "[Xen3d] Failed to receive socket traffic (return %d, errno %d)\n", sent, errno);
	    return 0;
	}
	bytessent += sent;
    }
    
    return 1;

}

static int send_message(int fd, char* message, int size) {

    int bytessent = 0;
    int sent;

    while(bytessent < size) {
	if((sent = send(fd, message + bytessent, size - bytessent, 0)) <= 0) {
	    fprintf(stderr, "[Xen3d] Failed to send socket traffic to the compositor in dpy_update (return %d, errno %d)\n", sent, errno);
	    return 0;
	}
	bytessent += sent;
    }
    
    return 1;

}

static int send_message_blocking(int fd, char* message, int size) {

    int flags = fcntl(sockfd, F_GETFL);
    
    flags &= ~O_NONBLOCK;
    
    fcntl(sockfd, F_SETFL, flags);
    
    send_message(fd, message, size);
    
    flags |= O_NONBLOCK;
    
    fcntl(sockfd, F_SETFL, flags);

}

union fdmsg {
    struct cmsghdr h;
    char buf[CMSG_SPACE(sizeof(int))];
};

static int sendfd(int tosend, int destination) {

    struct iovec iov;
    struct msghdr msg;

    char data = 'm';

    union fdmsg cmsg;
    struct cmsghdr* h;

    msg.msg_control = cmsg.buf;
    msg.msg_controllen = sizeof(union fdmsg);
    msg.msg_name = 0;
    msg.msg_namelen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_flags = 0;

    iov.iov_base = &data;
    iov.iov_len = 1;

    h = CMSG_FIRSTHDR(&msg);
    h->cmsg_len = CMSG_LEN(sizeof(int));
    h->cmsg_level = SOL_SOCKET;
    h->cmsg_type = SCM_RIGHTS;
    *((int*)CMSG_DATA(h)) = tosend;

    // For simplicity's sake, do this blocking

    int oldflags = fcntl(destination, F_GETFL);

    int newflags = oldflags & ~O_NONBLOCK;

    fcntl(destination, F_SETFL, newflags);

    int ret = sendmsg(destination, &msg, 0);

    if(ret == -1) {
        fprintf(stderr, "[Xen3d] Failed sending an FD to remote: errno = %d\n", errno);
        return 0;
    }

    // Reassert nonblocking mode, if the FD had it to begin with

    fcntl(destination, F_SETFL, oldflags);

    return 1;

}

static int recreate_format(DisplayState* ds) {

    if(source_format)
	Hermes_FormatFree(source_format);

    int32 rmask, gmask, bmask, amask = 0;

    switch(ds->depth) {
    
	case 32:
	    amask = 0xFF000000;
	    // Fall-through
	case 24:
	    rmask = 0x00FF0000;
	    gmask = 0x0000FF00;
	    bmask = 0x000000FF;
	    break;
	case 16:
	    rmask = 0x0000F800;
	    gmask = 0x000007E0;
	    bmask = 0x0000001F;
	    break;
	default:
	    rmask = gmask = bmask = 0;
	    
    }
    
    char indexed = (char)(ds->depth == 8);
    
    source_format = Hermes_FormatNew(ds->depth, rmask, gmask, bmask, amask, indexed);
    
    if(!source_format) {
	fprintf(stderr, "[Xen3d] Failed to create a format (%x, %x, %x, %x, depth %d, indexed = %d)\n",
			 rmask, gmask, bmask, amask, ds->depth, indexed);
	return 0;
    }
    
    return 1;

}

static int create_and_notify_new_buffer(int w, int h) {

    int oldsize = screen_width * screen_height * shm_pixel_bytes;

    screen_width = w;
    screen_height = h;

    int size = w * h * shm_pixel_bytes;

    if(shmtarget) {
	munmap(shmtarget, oldsize);
	shmtarget = 0;
    }
    
    // Find a creatable SHM section
    char sectionname[15];

    int sectionfd = -1;
    int sectionid = 0;

    while(sectionfd == -1 && sectionid < 100000) {
	sprintf(sectionname, "/xen3dfb-%d", sectionid);
	sectionfd = shm_open(sectionname, O_CREAT | O_EXCL | O_RDWR, S_IRWXU);
	if(sectionfd == -1 && errno != EEXIST) {
	  fprintf(stderr, "[Xen3d] Failed to shm_open: errno = %d\n", errno);
	  break;
	}
	sectionid++;
    }

    if(sectionfd == -1) {

	fprintf(stderr, "[Xen3d] Failed to get shared memory segment\n");
	return 0;
	
    }

    shm_unlink(sectionname);

    fprintf(stderr, "[Xen3d] Successfully created and mapped SHM with name %s\n", sectionname);

    int ret = ftruncate(sectionfd, size);

    if(ret == -1) {
	close(sectionfd);
	fprintf(stderr, "[Xen3d] Failed to ftruncate SHM at %s to size %d: errno = %d\n", sectionname, size, errno);
	return 0;
    }

    shmtarget = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, sectionfd, 0);

    if(shmtarget == (void*)-1) {
    
	fprintf(stderr, "[Xen3d] Failed to attach shared memory segment\n");
	close(sectionfd);
	return 0;
	
    }
	    
    struct new_buffer_message message;
    
    message.opcode = MSG_BUFFER;
    message.width = w;
    message.height = h;
    
    if(!send_message(sockfd, (char*)&message, sizeof(struct new_buffer_message))) {
	fprintf(stderr, "[Xen3d] Couldn't notify remote of a new buffer\n");
	munmap(shmtarget, size);
	shmtarget = 0;
	close(sectionfd);
	return 0;
    }

    if(!sendfd(sectionfd, sockfd)) {
        fprintf(stderr, "[Xen3d] Couldn't send section fd to remote\n");
        munmap(shmtarget, size);
        shmtarget = 0;
        close(sectionfd);
        return 0;
    }

    close(sectionfd);
    
    return 1;
    
}    
	    
static void xen3d_update(DisplayState *ds, int x, int y, int w, int h)
{
    //    printf("updating x=%d y=%d w=%d h=%d\n", x, y, w, h);
    // Copy and pixel-convert into shared area using Hermes

    // First some sanity checks:
    if(!shmtarget) {
	fprintf(stderr, "xen3d_update called with no mapped shared section; discarding\n");
	return;
    }
    if(!ds->data) {
	fprintf(stderr, "xen3d_update called with no ds->data; discarding\n");
	return;
    }
    
    struct sembuf request;
    
    request.sem_num = 0;
    request.sem_op = -1;
    request.sem_flg = 0;
    
    if(semop(compositor_semaphore, &request, 1) == -1) {
	fprintf(stderr, "[Xen3d] Failed to get lock in dpy_update\n");
	xen3d_die(ds);
	return;
    }
    
    // Locked the region we're drawing into
    
    if(!Hermes_ConverterRequest(hermes_handle, source_format, shm_format)) {
	fprintf(stderr, "[Xen3d] ConverterRequest failed in dpy_update\n");
	xen3d_die(ds);
	return;
    }
    
    if(ds->depth == 8) {
	if(!Hermes_ConverterPalette(hermes_handle, hermes_palette, hermes_palette)) {
	    fprintf(stderr, "[Xen3d] Failed to set palette for conversion in dpy_update\n");
	    xen3d_die(ds);
	    return;
	}
    }
    
    if(!Hermes_ConverterCopy(hermes_handle, 
	ds->data, x, y, w, h, ds->linesize, 
	shmtarget, x, y, w, h, ds->width * shm_pixel_bytes)) {
	    fprintf(stderr, "[Xen3d] Conversion failed in dpy_update\n");
	    xen3d_die(ds);
	    return;
    }
    
out:
    
    request.sem_op = 1;
    
    if(semop(compositor_semaphore, &request, 1) == -1) {
	fprintf(stderr, "[Xen3d] Failed to release lock in dpy_update\n");
	xen3d_die(ds);
	return;
    }

    struct update_message message;
    
    message.opcode = MSG_UPDATE;
    message.x = x;
    message.y = y;
    message.w = w;
    message.h = h;

    if(!send_message(sockfd, (char*)&message, sizeof(struct update_message))) {
	fprintf(stderr, "[Xen3d] Failed to send update notification to the compositor\n");
	xen3d_die(ds);
	return;
    }

}

static void xen3d_setdata(DisplayState *ds, void *pixels)
{

    ds->data = pixels;

    if(my_buffer) {
	free(my_buffer);
	my_buffer = 0;
    }
    
    if(hermes_palette) {
	Hermes_PaletteReturn(hermes_palette);
	hermes_palette = 0;
    }

    if (ds->depth == 8 && ds->palette != NULL) {
    
        hermes_palette = Hermes_PaletteInstance();
	if(!hermes_palette) {
	    fprintf(stderr, "[Xen3d] Could not create a Hermes palette in dpy_setdata\n");
	    xen3d_die(ds);
	    return;
	}

	int32* palette = Hermes_PaletteGet(hermes_palette);
	
	if(!palette) {
	    fprintf(stderr, "[Xen3d] Could not map palette in dpy_setdata\n");
	    xen3d_die(ds);
	    return;
	}
	
        int i;
        for (i = 0; i < 256; i++) {
            uint8_t rgb = ds->palette[i] >> 16;
	    struct pixel* entry = (struct pixel*)&(palette[i]);
            entry->r = ((rgb & 0xe0) >> 5) * 255 / 7;
            entry->g = ((rgb & 0x1c) >> 2) * 255 / 7;
            entry->b = (rgb & 0x3) * 255 / 3;
	    entry->a = 0xFF;
        }

	Hermes_PaletteInvalidateCache(hermes_handle);	
	
    }
    
    if(!recreate_format(ds) || !create_and_notify_new_buffer(ds->width, ds->height)) {
    
	xen3d_die(ds);
	return;
	
    }

}

static void xen3d_resize_shared(DisplayState *ds, int w, int h, int depth, int linesize, void *pixels)
{

    // Meaning: Use this pixels as the new ds->data, where it has the following dimensions
    //    printf("resizing to %d %d\n", w, h);

    if (!depth || !ds->depth) {
	ds->shared_buf = 0;
    }
    else {
	ds->shared_buf = 1;
	ds->depth = depth;
    }

    ds->width = w;
    ds->height = h;

    if(my_buffer) {
        free(my_buffer);
	my_buffer = 0;
    }

    if (!ds->shared_buf) {
	// I should own the buffer. Create one and set ->data to that.
        ds->depth = shm_pixel_bytes * 8;
        ds->bgr = 0;
	
	my_buffer = malloc(w * h * shm_pixel_bytes);
	
	if(!my_buffer) {
	    fprintf(stderr, "[Xen3d] malloc failed in dpy_resize_shared, trying for a display of %dx%dx%d\n", w, h, shm_pixel_bytes);
	    xen3d_die(ds);
	    return;
	}
        ds->data = my_buffer;
        ds->linesize = w * shm_pixel_bytes;

	if(!recreate_format(ds) || !create_and_notify_new_buffer(w, h)) {
	
	    xen3d_die(ds);
	    return;
	
	}
	
    } else {
        ds->linesize = linesize;
        ds->dpy_setdata(ds, pixels);
    }

}

static void xen3d_resize(DisplayState *ds, int w, int h)
{

  int pix_bytes;
  if(ds->depth == 24) {
    pix_bytes = 4;
  }
  else {
    pix_bytes = ds->depth / 8;
  }
    
    xen3d_resize_shared(ds, w, h, 0, w * pix_bytes, NULL);

}

/* generic keyboard conversion */
/*
#include "keymaps.c"

static kbd_layout_t *kbd_layout = NULL;

static uint8_t sdl_keyevent_to_keycode_generic(const SDL_KeyboardEvent *ev)
{
    int keysym;*/
    /* workaround for X11+SDL bug with AltGR */
/*    keysym = ev->keysym.sym;
    if (keysym == 0 && ev->keysym.scancode == 113)
        keysym = SDLK_MODE;*/
    /* For Japanese key '\' and '|' */
/*    if (keysym == 92 && ev->keysym.scancode == 133) {
        keysym = 0xa5;
    }
    return keysym2scancode(kbd_layout, keysym);
}*/

/* specific keyboard conversions from scan codes */

static uint8_t xen3d_x_to_qemu_keycode(int keycode)
{
    if (keycode < 9) {
        keycode = 0;
    } else if (keycode < 97) {
        keycode -= 8; /* just an offset */
    } else if (keycode < 212) {
        /* use conversion table */
        keycode = _translate_keycode(keycode - 97);
    } else {
        keycode = 0;
    }
    return keycode;
}

/*
static void reset_keys(void)
{
    int i;
    for(i = 0; i < 256; i++) {
        if (modifiers_state[i]) {
            if (i & 0x80)
                kbd_put_keycode(0xe0);
            kbd_put_keycode(i | 0x80);
            modifiers_state[i] = 0;
        }
    }
}
*/

static void xen3d_process_key(int keycode, int down)
{

//    switch(keycode) {
/*    case 0x00:*/
        /* sent when leaving window: reset the modifiers state */
/*        reset_keys();
        return;*/
//    case 0x2a:                          /* Left Shift */
//    case 0x36:                          /* Right Shift */
//    case 0x1d:                          /* Left CTRL */
//    case 0x9d:                          /* Right CTRL */
//    case 0x38:                          /* Left ALT */
//    case 0xb8:                         /* Right ALT */
/*        if (ev->type == SDL_KEYUP)
            modifiers_state[keycode] = 0;
        else
            modifiers_state[keycode] = 1;
        break;*/
//    case 0x45: /* num lock */
//    case 0x3a: /* caps lock */
        /* SDL does not send the key up event, so we generate it */
/*        kbd_put_keycode(keycode);
        kbd_put_keycode(keycode | 0x80);
        return;
    }*/

    /* now send the key code */
    
    char newkey = xen3d_x_to_qemu_keycode(keycode);
    
    if (newkey & 0x80)
        kbd_put_keycode(0xe0);
    if (!down)
        kbd_put_keycode(newkey | 0x80);
    else
        kbd_put_keycode(newkey & 0x7f);
}

static void xen3d_send_mouse_event(int absx, int absy, int state)
{

    int repx, repy;

    if (kbd_mouse_is_absolute()) {

	repx = absx * (0x7FFF / (screen_width - 1));
	repy = absy * (0x7FFF / (screen_height - 1));

    } else {

	repx = absx - mouse_last_x;
	repy = absy - mouse_last_y;

    }

    kbd_mouse_event(repx, repy, 0, state);
    
}

static int get_message_size(int opcode) {

    switch(opcode) {
    
	case XEN3D_QEMU_KEY_EVENT:
	return sizeof(struct xen3d_qemu_key_event);
	
	case XEN3D_QEMU_BUTTON_EVENT:
	return sizeof(struct xen3d_qemu_button_event);
	
	case XEN3D_QEMU_MOTION_EVENT:
	return sizeof(struct xen3d_qemu_motion_event);
	
	default:
	return -1;
	
    }
    
}

static void execute_input_command(char* command, int length) {

    int opcode = *(int*)command;
    
    switch(opcode) {
    
	case XEN3D_QEMU_KEY_EVENT:
	{
	    struct xen3d_qemu_key_event* event = (struct xen3d_qemu_key_event*)command;
	    xen3d_process_key((char)event->scancode, event->down);
	    break;
	}
	case XEN3D_QEMU_BUTTON_EVENT:
	{
	    struct xen3d_qemu_button_event* event = (struct xen3d_qemu_button_event*)command;
	    int mask;
	    if(event->button == 1)
		mask = MOUSE_EVENT_LBUTTON;
	    else if(event->button == 2)
		mask = MOUSE_EVENT_MBUTTON;
	    else if(event->button == 3)
		mask = MOUSE_EVENT_RBUTTON;
	    else
		break;
		
	    if(event->down)
		mouse_state |= mask;
	    else
		mouse_state &= ~mask;
		
	    xen3d_send_mouse_event(mouse_last_x, mouse_last_y, mouse_state);		
	    
	    break;
	    
	}
	
	case XEN3D_QEMU_MOTION_EVENT:
	{
	    struct xen3d_qemu_motion_event* event = (struct xen3d_qemu_motion_event*)command;
	    xen3d_send_mouse_event(event->x, event->y, mouse_state);
	    
	    mouse_last_x = event->x;
	    mouse_last_y = event->y;
	    
	    break;
	    	    
	}
	
    }
    
}

static void xen3d_refresh(DisplayState *ds)
{

    while(true) {

	ssize_t bytes_needed = input_target - input_current;
	ssize_t new_bytes = recv(sockfd, input_current, bytes_needed, 0);
    
        if(new_bytes == -1 && errno == EAGAIN)
    	    return;
	    // Buffer is empty
	else if(new_bytes <= 0) {
	    // Socket closed or broken
	    fprintf(stderr, "[Xen3d] Socket error getting input commands\n");
	    xen3d_die(ds);
	    return;
	}
	// else, we got some data...
    
	if(new_bytes == bytes_needed) {

	    ssize_t total_bytes = input_target - input_command;
	    
	    if(total_bytes == sizeof(int)) {
	    
		ssize_t new_bytes_needed = get_message_size(*(int*)input_command);
		if(new_bytes_needed == -1) {
		
		    fprintf(stderr, "[Xen3d] Received invalid opcode: %d\n", *(int*)input_command);
		    xen3d_die(ds);
		    return;
		    
		}
		
		input_command = realloc(input_command, new_bytes_needed);
		input_current = input_command + total_bytes;
		input_target = input_command + new_bytes_needed;
		
	    }
	    else {
	    
		execute_input_command(input_command, total_bytes);
		input_current = input_command;
		input_target = input_current + sizeof(int);
		
	    }    

	}
	else {
	
	    input_current += new_bytes;
	
	}

    }

}

static void xen3d_cleanup(void) 
{

    if(my_buffer) {
	free(my_buffer);
	my_buffer = 0;
    }
	
    if(hermes_handle) {
	Hermes_ConverterReturn(hermes_handle);
	hermes_handle = 0;
    }
	
    if(hermes_palette) {
	Hermes_PaletteReturn(hermes_palette);
	hermes_palette = 0;
    }
	
    if(source_format) {
	Hermes_FormatFree(source_format);
	source_format = 0;
    }
	
    if(shm_format) {
	Hermes_FormatFree(shm_format);
	shm_format = 0;
    }
    
    if(compositor_semaphore != -1) {
	semctl(compositor_semaphore, IPC_RMID, 0);
	compositor_semaphore = -1;
    }
    
    if(shmtarget != 0) {
	munmap(shmtarget, screen_width * screen_height * shm_pixel_bytes);
	shmtarget = 0;    
    }
    
    if(input_command) {
        free(input_command);
	input_command = 0;
    }
	
    if(!Hermes_Done()) {
	fprintf(stderr, "[Xen3d] Hermes_Done returned an error; refcounting mistake?\n");
    }
    
    close(sockfd);

}

static void dumb_update(DisplayState *ds, int x, int y, int w, int h)
{
}

static void dumb_resize(DisplayState *ds, int w, int h)
{
}

static void dumb_refresh(DisplayState *ds)
{
}

static void dumb_resize_shared(DisplayState *ds, int w, int h, int depth, int linesize, void *pixels) {

  ds->data = pixels;

}

static void dumb_setdata(DisplayState* ds, void* pixels) {

  ds->data = pixels;

}

static void xen3d_die(DisplayState* ds) {

  fprintf(stderr, "Dying\n");

    xen3d_cleanup();

    ds->dpy_update = dumb_update;
    ds->dpy_resize = dumb_resize;
    ds->dpy_refresh = dumb_refresh;
    ds->dpy_setdata = dumb_setdata;
    ds->dpy_resize_shared = dumb_resize_shared;
    ds->linesize = 0;
    ds->depth = 0;
    ds->gui_timer_interval = 500;
    ds->idle = 1;

}

void xen3d_display_init(DisplayState *ds)
{

    if(!Hermes_Init()) {
	fprintf(stderr, "[Xen3d] Failed to initialise Hermes pixel-conversion library\n");
	return;
    }
    
    if(!(hermes_handle = Hermes_ConverterInstance(0))) {
	fprintf(stderr, "[Xen3d] Failed to get Hermes converter\n");
	xen3d_die(ds);
	return;
    }
    
        
    key_t key = 35000;
    
    while(((compositor_semaphore = semget(key, 1, IPC_CREAT | IPC_EXCL | 0600)) == -1)
	   && errno == EEXIST)
		key++;
		
    if(compositor_semaphore == -1) {
	fprintf(stderr, "[Xen3d] Failed to get semaphore set\n");
	xen3d_die(ds);
	return;
    }
    
    int before = semctl(compositor_semaphore, 0, GETVAL);
    
    if(semctl(compositor_semaphore, 0, SETVAL, 1) == -1) {
	fprintf(stderr, "[Xen3d] Couldn't set semaphore initial value\n");
	xen3d_die(ds);
	return;
    }
    
    int after = semctl(compositor_semaphore, 0, GETVAL);
    
    fprintf(stderr, "[Xen3d] Opened semaphore with key %d\n", key);
    
    if((sockfd = socket(PF_UNIX, SOCK_STREAM, 0)) == -1) {
	fprintf(stderr, "[Xen3d] Couldn't create a Unix domain socket\n");
	xen3d_die(ds);
	return;
    }
    
    struct sockaddr_un sun;
    
    sun.sun_family = AF_UNIX;
    strcpy(sun.sun_path, "/var/run/xen3d-socket");
    
    if(connect(sockfd, &sun, sizeof(sun.sun_family) + strlen(sun.sun_path)) < 0) {
	fprintf(stderr, "[Xen3d] Couldn't connect to /var/run/xen3d-socket\n");
	xen3d_die(ds);
	return;
    }
    
    struct init_message message;
    message.opcode = MSG_INIT;
    message.sem_key = key;
    message.domain = domid;
    
    if(!send_message(sockfd, (char*)&message, sizeof(struct init_message))) {
	fprintf(stderr, "[Xen3d] Couldn't connect to /var/run/xen3d-socket\n");
	xen3d_die(ds);
	return;
    }    
    
    // Now wait for a message telling us the depth the compositor desires.
    // Currently valid: 32 and 16, assumed to be 8/8/8/8 and 5/6/5 respectively.
    
    struct display_state_message depthinfo;
    
    int success = recv_all(sockfd, (char*)&depthinfo, sizeof(struct display_state_message));
    
    if(!success) {
	fprintf(stderr, "[Xen3d] Failed receiving from compositor socket\n");
	xen3d_die(ds);
	return;
    }    

    shm_depth = (depthinfo.depth / 8);

    if(shm_depth == 4) {
      shm_pixel_bytes = 4;
	shm_format = Hermes_FormatNew(32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000, 0);
        fprintf(stderr, "[Xen3d] Configured for depth 32\n");
    }
    else if(shm_depth == 3) {
      shm_pixel_bytes = 4;
      shm_format = Hermes_FormatNew(32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0, 0);
      fprintf(stderr, "[Xen3d] Configured for non-packed depth 24 (pixel stride 32)\n");
    }
    else if(shm_depth == 2) {
      shm_pixel_bytes = 2;
        shm_format = Hermes_FormatNew(16, 0x0000F800, 0x000007E0, 0x0000001F, 0, 0);
	fprintf(stderr, "[Xen3d] Configured for depth 16\n");
    }

    if(!shm_format) {
    
	fprintf(stderr, "[Xen3d] Failed to create format with depth %d (only 4 and 2 currently supported)\n", shm_depth);
	xen3d_die(ds);
	return;

    }
    
    input_command = malloc(sizeof(int));
    input_current = input_command;
    input_target = input_command + sizeof(int);
    
    if(!input_command) {
    
	fprintf(stderr, "[Xen3d] Failed to malloc initial buffer for input commands\n");
	xen3d_die(ds);
	return;
	
    }
    
    // Finally make the socket non-blocking, so xen3d_refresh doesn't block indefinitely
    
    int flags = fcntl(sockfd, F_GETFL);
    
    flags |= O_NONBLOCK;
    
    fcntl(sockfd, F_SETFL, flags);

    // Everything worked!
    
    ds->dpy_update = xen3d_update;
    ds->dpy_resize = xen3d_resize;
    ds->dpy_resize_shared = xen3d_resize_shared;
    ds->dpy_refresh = xen3d_refresh;
    ds->dpy_setdata = xen3d_setdata;

    xen3d_resize(ds, 640, 400);
}

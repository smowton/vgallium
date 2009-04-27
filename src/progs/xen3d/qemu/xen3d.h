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

#include <sys/types.h>
#include <sys/ipc.h>

#define MSG_UPDATE 0
#define MSG_BUFFER 1
#define MSG_INIT 2
#define MSG_DISPLAY_STATE 3

struct new_buffer_message {
    
    int opcode;
    int width;
    int height;
    
};

struct update_message {
    
    int opcode;
    int x;
    int y;
    int w;
    int h;
    
};

struct init_message {

  int opcode;
  key_t sem_key;
  int domain;
    
};

struct display_state_message {

    int opcode;
    int depth;
    
};

#define XEN3D_QEMU_KEY_EVENT 1
#define XEN3D_QEMU_BUTTON_EVENT 2
#define XEN3D_QEMU_MOTION_EVENT 3

struct xen3d_qemu_key_event {

    int opcode;
    int scancode;
    int down;
    
};

struct xen3d_qemu_button_event {

    int opcode;
    int button;
    int down;
    
};

struct xen3d_qemu_motion_event {

    int opcode;
    int x;
    int y;
    
};

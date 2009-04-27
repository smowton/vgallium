
// Linux-specific headers
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/timeb.h>

// C standard library
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>

// Xen headers
#include <xenctrl.h>
#include <xs.h>

// Xorg headers (including the Xen3d extension)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/xen3d_extproto.h>
#include <xorg/regionstr.h>
#include <xorg/miscstruct.h>

// EGL headers
#include <EGL/egl.h>

// Hermes headers
#include <Hermes/Hermes.h>

// Internal headers
#include "pipe/p_context.h"
#include "pipe/p_inlines.h"

#include "rawgal.h"

#include "client.h"
#include "dispatch.h"
#include "master_context.h"
#include "remote_messages.h"
#include "picker.h"

// The header for our extension to QEMU
#include "xen3d.h"

#ifdef CS_DEBUG_MAIN
#include "debug.h"
#include <stdio.h>
#define DBG(format, args...) printf(format, ## args)
#else
#define DBG(format, args...)
#endif

#ifdef CS_DEBUG_DESTROY
#include "debug.h"
#include <stdio.h>
#define DBGDE(format, args...) printf(format, ## args)
#else
#define DBGDE(format, args...)
#endif

#ifdef CS_DEBUG_SELECT
#include "debug.h"
#include <stdio.h>
#define DBGSEL(format, args...) printf(format, ## args)
#else
#define DBGSEL(format, args...)
#endif

#ifdef CS_DEBUG_QEMU
#include "debug.h"
#include <stdio.h>
#define DBGQEMU(format, args...) printf(format, ## args)
#else
#define DBGQEMU(format, args...)
#endif

#ifdef CS_DEBUG_INPUT
#include "debug.h"
#include <stdio.h>
#define DBGINPUT(format, args...) printf(format, ## args)
#else
#define DBGINPUT(format, args...)
#endif

#ifdef CS_DEBUG_TCP
#include "debug.h"
#include <stdio.h>
#define DBGTCP(format, args...) printf(format, ## args)
#else
#define DBGTCP(format, args...)
#endif

#ifdef CS_DEBUG_XIDC
#include "debug.h"
#include <stdio.h>
#define DBGXIDC(format, args...) printf(format, ## args)
#else
#define DBGXIDC(format, args...)
#endif

#define MAX_GALLIUM_CMDS_BEFORE_YIELD 100
#define MAX_X_CMDS_BEFORE_YIELD 5
#define MAX_QEMU_CMDS_BEFORE_YIELD 5

// Xenstore path under which clients must create a key to request a connnection
#define WATCH_NODE "/local/domain/0/backend/xen3d/requests"

int destroy_qemu(struct domain_list_entry*, struct domain_list_entry**, struct global_state*);

// Maybe consider eliminating a copy with some sort of alloc-in-ring method.
void* allocate_message_memory(size_t bytes) {

  struct message_header* message = (struct message_header*)malloc(bytes);

  message->length = bytes;

  return (void*)message;

}

// Send or queue: attempt to immediately dispatch a message using a given
// receiver; if it can't be done immediately, queue it.
// Return 1 for success (or unknown result -- i.e. the send is queued)
// Return 0 for failure.

// Assumption: there is not anything queued right now.
// This works, as all request-reply pairs in Gallium are synchronous for the
// driver, so he will not send more messages when expecting a reply to his
// previous one. Further, in the select loop (main_loop) I will not process
// any receive data whilst there is a send pending.

int send_or_queue(struct receiver* rx, char* data, int length) {

  if(!rx->send_arg)
    return 0;

  int sent = rx->send(rx->send_arg, data, length);

  if(sent == length) {
    // The simple case -- it's done with, free and return success
    free(data);
    return 1;
  }
  else if(sent >= 0) {
    rx->partial_send = data;
    rx->partial_send_length = length;
    rx->bytes_sent = sent;
    return 1;
  }
  else {
    return 0;
  }

}

// Non-blocking send: try to send a given data item; return the number of bytes
// sent, or -1 on error. Will be called from send_or_queue and subsequently
// from the select loop.

int xidc_send(void* arg, char* data, int length) {

  struct xidc_receiver_state* state = (struct xidc_receiver_state*)arg;

  char* buffer = (char*)(&(((uint32_t*)state->return_buffer)[3]));
  uint32_t* readoff = &(((uint32_t*)state->return_buffer)[1]);
  uint32_t* writeoff = &(((uint32_t*)state->return_buffer)[2]);
  uint32_t buffersize = *(uint32_t*)state->return_buffer;

  int bytes_sent = 0;

  while(bytes_sent < length) {
    int space = 0;
    uint32_t limit = *readoff;
    uint32_t base = *writeoff;
    int writewrapsbuffer;

    DBGXIDC("On going around, read = %u, write = %u\n", limit, base);
    
    if(limit > base) {
      space = (limit - base) - 1;
    }
    else {
      space = base - limit;
      space = (buffersize - space) - 1;
      // Care! > rather than >= because if it == buffersize, the write does   
      // not wrap, but the pointer does                                       
    }

    if(space == 0) {
      // Out of space before completed sending; return and wait.
      return bytes_sent;
    }

    int towrite = space < (length - bytes_sent) ? space : (length - bytes_sent);

    if((base + towrite) > buffersize)
      writewrapsbuffer = 1;
    else
      writewrapsbuffer = 0;

    DBGXIDC("Determined this means we can write %d bytes\n", towrite);

    if(writewrapsbuffer) {
      DBGXIDC("The write will wrap the ring\n");     
    }

    if(!writewrapsbuffer) {
      DBGXIDC("Writing to ring offset %u\n", base);                
      memcpy(&(buffer[base]), data, towrite);
    }
    else {

      int bytestoend = buffersize - base;
      memcpy(&(buffer[base]), data, bytestoend);
      DBGXIDC("Copied %d bytes to ring offset %u\n", bytestoend, base);
      memcpy(buffer, data + bytestoend, towrite - bytestoend);
      DBGXIDC("Copied %d bytes to ring offset 0\n", towrite - bytestoend);

    }

    bytes_sent += towrite;
    data += towrite;

    uint32_t newwriteoff = base + towrite;
    if(newwriteoff >= buffersize)
      newwriteoff -= buffersize;

    *writeoff = newwriteoff;

    // Notify of the new data available
    if(xc_evtchn_notify(state->evtchnhandle, state->return_evtid) == -1) {
      printf("Failed to notify event channel %d whilst sending\n", state->return_evtid);
      return -1;
    }

  }

  return length;

}

// Blocking send: transmit all or nothing. Return 0 on failure, 1 on success.

int tcp_send_blocking(void* arg, char* data, int length) {

  struct tcp_receiver_state* state = (struct tcp_receiver_state*)arg;

  ssize_t bytes_sent = 0;

  while(bytes_sent < length) {
    
    ssize_t this_send = send(state->fd, data + bytes_sent, length - bytes_sent, 0);
    if(this_send == -1) {
      if(errno != EAGAIN) {
	printf("Send failed (sending %d bytes, got return %d, total sent %d)\n", length, this_send, bytes_sent);
	return 0;
      }
    }
    else if(this_send == 0) {
      printf("Orderly close of socket during send_message; failed\n");
      return 0;
    }
    else {
      bytes_sent += this_send;
    }

  }
  
  return 1;

}

int send_message(struct client_list_entry* client, void* data) {

  struct message_header* msg = (struct message_header*)data;

  DBG("Sending message %s (length %u)\n", optotext(msg->opcode), msg->length);
  
  return send_or_queue(&(client->receiver), (char*)data, msg->length);

}


void setnonblocking(int sock) {
  
  int opts;
  
  opts = fcntl(sock,F_GETFL);
  if (opts < 0) {
    printf("Failed to set socket non-blocking\n");
    exit(1);
  }
  opts = (opts | O_NONBLOCK);
  if (fcntl(sock,F_SETFL,opts) < 0) {
    printf("Failed to set socket non-blocking\n");
    exit(1);
  }
  return;
  
}

void send_input_message_to_domain(struct global_state* global_state, struct domain_list_entry* domain, char* msg, int len) {

    if(domain) {

      struct receiver* qrx = &(domain->qemu_receiver);
      void* send_arg = qrx->send_arg;
    
      if(send_arg) {
    
	qrx->send_blocking(qrx->send_arg, msg, len);
	
      }
      else {
	DBGINPUT("Dropped an input message (no Qemu connection for the input domain)\n");
      }
	
    }
    else {
      DBGINPUT("Dropped an input message (no current input domain)\n");
    }
    
}

void send_input_message(struct global_state* global_state, char* msg, int len) {
 
  send_input_message_to_domain(global_state, global_state->current_domain, msg, len);

}

void send_trusted_input_message(struct global_state* global_state, char* msg, int len) {

  send_input_message_to_domain(global_state, global_state->trusted_domain_descriptor, msg, len);

}

int handle_x_events(struct global_state* global_state) {

    XEvent next_event;

    int should_notify = 0;

    while(XPending(global_state->x_display)) {
    
	XNextEvent(global_state->x_display, &next_event);

	if(global_state->control_dom_state != CONTROL_DOM_STATE_PENDING) {
	
	  switch(next_event.type) {
	
	  case KeyPress:
	  case KeyRelease:
	    {
	      struct xen3d_qemu_key_event message;
	      message.opcode = XEN3D_QEMU_KEY_EVENT;
	      message.scancode = (int)next_event.xkey.keycode;
	      message.down = (next_event.type == KeyPress);
	    
	      if(global_state->control_dom_state == CONTROL_DOM_STATE_VISIBLE) {
		send_trusted_input_message(global_state, (char*)&message, sizeof(struct xen3d_qemu_key_event));
	      }
	      else {
		send_input_message(global_state, (char*)&message, sizeof(struct xen3d_qemu_key_event));
	      }
	    
	      DBGINPUT("Pressed/released key with code %d\n", (int)next_event.xkey.keycode);
	      break;
	    }
	  case ButtonPress:
	  case ButtonRelease:
	    {
	      struct xen3d_qemu_button_event message;
	      message.opcode = XEN3D_QEMU_BUTTON_EVENT;
	      message.button = (int)next_event.xbutton.button;
	      message.down = (next_event.type == ButtonPress);
	    
	      if(message.button == 2) {
		if(message.down) {
		  if(global_state->control_dom_state == CONTROL_DOM_STATE_INVISIBLE)
		    should_notify = 1;
		}
	      }
	      else {
		if(global_state->control_dom_state == CONTROL_DOM_STATE_VISIBLE) {
		  send_trusted_input_message(global_state, (char*)&message, sizeof(struct xen3d_qemu_button_event));
		}
		else {
		  send_input_message(global_state, (char*)&message, sizeof(struct xen3d_qemu_button_event));
		}
	      }

	      DBGINPUT("Pressed/released mouse button %d\n", next_event.xbutton.button);
	      break;
	    }
	  case MotionNotify:
	    {
	      struct xen3d_qemu_motion_event message;
	      message.opcode = XEN3D_QEMU_MOTION_EVENT;
	      message.x = next_event.xmotion.x;
	      message.y = next_event.xmotion.y;

	      // TODO: Figure out how to express mouse-moves when we're outside the clipped area, offsets etc.

	      if(global_state->control_dom_state == CONTROL_DOM_STATE_VISIBLE) {
		send_trusted_input_message(global_state, (char*)&message, sizeof(struct xen3d_qemu_motion_event));
	      }
	      else {
		send_input_message(global_state, (char*)&message, sizeof(struct xen3d_qemu_motion_event));
	      }
	    
	      DBGINPUT("Motion: x=%d, y=%d\n", next_event.xmotion.x, next_event.xmotion.y);
	      break;
	    }
	  default:
	  
	    DBGINPUT("Discarded an event of type %d\n", next_event.type);
	    
	  }

	}
	
    }

    return should_notify;

}

void debug_print_bytes(char* bytes, int nbytes) {

  int i;
  for(i = 0; i < nbytes; i++) {
    DBGXIDC("%x ", (unsigned int)((unsigned char)bytes[i]));
    if(i && (!(i % 8)))
      DBGXIDC("\n");
  }

  DBGXIDC("\n");

}

int xidc_receive(void* arg, char* userbuffer, int size) {

  struct xidc_receiver_state* state = (struct xidc_receiver_state*)arg;

  uint32_t buffersize = *((uint32_t*)state->buffer);
  uint32_t* readoff = &(((uint32_t*)state->buffer)[1]);
  uint32_t* writeoff = &(((uint32_t*)state->buffer)[2]);
  char* bufferstart = (char*)&(((uint32_t*)state->buffer)[3]);

  int userbufferspace = size;

  while(userbufferspace && (*readoff != *writeoff)) {

    uint32_t limit = *writeoff;
    uint32_t base = *readoff;
    int bytestoread;
    int wrapsbuffer = 0;
    
    DBGXIDC("On going around, read-offset is %u and write-offset is %u\n", base, limit);                                                              
    if(limit > base)
      bytestoread = limit - base;
    else
      bytestoread = buffersize - (base - limit);

    DBGXIDC("Determined that means we have %d bytes available to read\n", bytestoread);

    int bytestocopy = userbufferspace < bytestoread ? userbufferspace : bytestoread;

    DBGXIDC("Will copy %d bytes\n", bytestocopy);

    if(base + bytestocopy >= buffersize)
      wrapsbuffer = 1;

    if(!wrapsbuffer) {
      memcpy(userbuffer, &(bufferstart[base]), bytestocopy);
      *readoff += bytestocopy;
    }
    else {
      int bytestoend = buffersize - base;
      
      memcpy(userbuffer, &(bufferstart[base]), bytestoend);
      DBGXIDC("Read %d bytes from ring index %u\n", bytestoend, base);       
      memcpy(userbuffer + bytestoend, bufferstart, bytestocopy - bytestoend);
      DBGXIDC("Read %d bytes from ring index 0\n", bytestocopy - bytestoend);

      uint32_t newreadoff = base + bytestocopy;
      if(newreadoff >= buffersize)
	newreadoff -= buffersize;
      
      *readoff = newreadoff;
    }

    userbufferspace -= bytestocopy;
    userbuffer += bytestocopy;

  }

  DBGXIDC("Leaving xidc_receive (read: %u, write: %u)\n", *readoff, *writeoff);

#ifdef DBGXIDC
  if(!userbufferspace) {

    // We've stopped reading because the user's message is complete, not because they the ring is empty

    uint32_t limit = *writeoff;
    uint32_t base = *readoff;

    int remainingbytes;
    int wrapsbuffer = 0;

    if(limit > base)
      remainingbytes = limit - base;
    else
      remainingbytes = buffersize - (base - limit);
    DBGXIDC("Leaving ring-read with %d bytes still available. Dump:\n", remainingbytes);

    if(base + remainingbytes >= buffersize)
      wrapsbuffer = 1;

    if(!wrapsbuffer) {
      debug_print_bytes(&(bufferstart[base]), remainingbytes);
    }
    else {
      int bytestoend = buffersize - base;
      
      debug_print_bytes(&(bufferstart[base]), bytestoend);
      DBGXIDC("---ring end---\n");
      debug_print_bytes(bufferstart, remainingbytes - bytestoend);
    }
  }
#endif

  return size - userbufferspace;

}

int tcp_receive(void* arg, char* buffer, int size) {

  struct tcp_receiver_state* state = (struct tcp_receiver_state*)arg;

  ssize_t err = recv(state->fd, buffer, size, 0);
  
  if(err == -1 && errno == EAGAIN)
    return 0;
  else if(err <= 0)
    return -1;
  else
    return err;

}

int receiver_process_data (struct receiver* receiver) {

  int err;
  int updates_processed = 0;

  DBGTCP("Entered receive handler\n");
  DBGTCP("About to query for %d bytes\n", receiver->target - receiver->recv_current);

  while((updates_processed < receiver->max_updates_before_yield) 
	&& ((err = receiver->receive(receiver->receive_arg, receiver->recv_current, receiver->target - receiver->recv_current)) > 0)) {

    DBGTCP("Bytes received: %d\n", err);
	    
    receiver->recv_current += err;

    if(receiver->recv_current == receiver->target) {
	    
      DBGTCP("Reached current target of %d bytes\n", receiver->target - receiver->command);
		
      int bytes_received = receiver->recv_current - (char*)receiver->command;
      int total_bytes_required = receiver->process(receiver->command, bytes_received, receiver->process_arg);
		
      DBGTCP("Got a new target of %d\n", total_bytes_required);

      if(total_bytes_required == 0) {
	// Special return indicates this command is done with
	updates_processed++;
		    
	int initial_bytes_required = receiver->process(0, 0, receiver->process_arg);
	DBGTCP("Command processed and discarded; initial target for new command: %d bytes\n", initial_bytes_required);
		    
	receiver->command = realloc(receiver->command, initial_bytes_required);
	receiver->recv_current = receiver->command;
	receiver->target = receiver->command + initial_bytes_required;

      }
      else if(total_bytes_required == -1) {
	DBGTCP("Processing returned an error; passing to caller\n");
	// special return indicates an error occurred which we should
	// return to our caller
	return 1;
      }
      else {
	DBGTCP("Got %d, need %d\n", bytes_received, total_bytes_required);
	receiver->command = realloc(receiver->command, total_bytes_required);
	if(!receiver->command) {
	  printf("Receiver failed to allocate %d bytes\n", total_bytes_required);
	  return 1;
	}
	receiver->recv_current = receiver->command + bytes_received;
	receiver->target = receiver->command + total_bytes_required;
      }
		
    }

  }
    
  // Return of 0 from a receive method means the buffer is empty
  // Negative return means an irrecoverable transport error
  if(err < 0)
    return 1; // Transport error
  else if(err == 0)
    return 0; // Fully drained buffer
  else if(err > 0)
    return -1; // Partially drained buffer

}

#ifndef MIN
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

#ifndef MAX
#define MAX(x, y) ((x) < (y) ? (y) : (x))
#endif

union fdmsg {
    struct cmsghdr h;
    char buf[CMSG_SPACE(sizeof(int))];
};

int receive_fd(int fromfd) {

    struct iovec iov;
    struct msghdr msg;

    char data;

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
    *((int*)CMSG_DATA(h)) = -1;

    // For simplicity's sake, block for the duration of the FD receive.

    int oldflags = fcntl(fromfd, F_GETFL);

    int blockingflags = oldflags & ~O_NONBLOCK;

    fcntl(fromfd, F_SETFL, blockingflags);

    int ret = recvmsg(fromfd, &msg, 0);

    if(ret <= 0) {
        printf("Failed receiving a file descriptor: returned %d with errno %d\n", ret, errno);
        return -1;
    }

    // Restore the old flags

    fcntl(fromfd, F_SETFL, oldflags);

    h = CMSG_FIRSTHDR(&msg);
    // The website illustrating this process suggested the kernel might realloc
    // our buffer, though this seems illogical considering its size has not changed.

    return *((int*)CMSG_DATA(h));

}

int qemu_process(char* command, int length, void* arg) {

  if(!command) {
    DBGQEMU("Qemu-process: got an empty command, returning sizeof(int)\n");
    // To determine command length we need to know the first int
    return sizeof(int);
  }
  else if(length == sizeof(int)) {
    DBGQEMU("Qemu-process: determining command length given opcode %d\n", *(int*)command);
    // Can now determine the complete command length:
    switch(*(int*)command) {
    case MSG_UPDATE:
      return sizeof(struct update_message);
      break;
    case MSG_BUFFER:
      return sizeof(struct new_buffer_message);
      break;
    case MSG_INIT:
      return sizeof(struct init_message);
      break;
    default:
      printf("Received invalid opcode from Qemu-dm\n");
      return -1;
    }
  }
  else {
    DBGQEMU("Qemu-process: got a complete command\n");
    // Message is complete: process!
    struct domain_list_entry* dom = (struct domain_list_entry*)arg;
    switch(*(int*)command) {
    case MSG_UPDATE:
      // Just set the dirty rect to one including both the existing rect
      // and this new one.
      {
	DBGQEMU("Got an update message\n");
	struct update_message* msg = (struct update_message*)command;
	if((!dom->dirty_w) || (!dom->dirty_h)) {
	  DBGQEMU("Found no existing dirty-rect, using origin (%d, %d) dimension (%d, %d)\n", msg->x, msg->y, msg->w, msg->h);
	  // No existing dirty-rect
	  dom->dirty_x = msg->x;
	  dom->dirty_y = msg->y;
	  dom->dirty_w = msg->w;
	  dom->dirty_h = msg->h;
	}
	else {
	  int left = MIN(dom->dirty_x, msg->x);
	  int right = (MAX(dom->dirty_x + dom->dirty_w, msg->x + msg->w));
	  int top = MIN(dom->dirty_y, msg->y);
	  int bottom = (MAX(dom->dirty_y + dom->dirty_h, msg->y + msg->h));
		
	  DBGQEMU("Found existing dirty-rect of origin (%d, %d) dimension (%d, %d)\n", dom->dirty_x, dom->dirty_y, dom->dirty_w, dom->dirty_h);
	  DBGQEMU("Importing dirty-rect of origin (%d, %d) dimension (%d, %d)\n", msg->x, msg->y, msg->w, msg->h);

	  dom->dirty_x = left;
	  dom->dirty_w = (right - left);
	  dom->dirty_y = top;
	  dom->dirty_h = (bottom - top);

	  DBGQEMU("Merged to give dirty-rect of origin (%d, %d) dimension (%d, %d)\n", dom->dirty_x, dom->dirty_y, dom->dirty_w, dom->dirty_h);

	}
	break;
      }
    case MSG_BUFFER:
      // If there is an existing piece of mapped SHM, drop it and map
      // this new one
      {
	struct new_buffer_message* msg = (struct new_buffer_message*)command;

	DBGQEMU("Got a new-buffer message\n");

	int oldsize = dom->backdrop_width * dom->backdrop_height * (dom->global_state->window_pixel_size / 8);

	if(dom->qemu_shared_section) {
	  DBGQEMU("Found and released an existing shared section (%p)\n", dom->qemu_shared_section);
	  munmap(dom->qemu_shared_section, oldsize);
	  dom->qemu_shared_section = 0;
	}
	    
	int size = msg->width * msg->height * (dom->global_state->window_pixel_size / 8);

	printf("Domain %d resized 2D framebuffer. New size: %dx%d\n", dom->domain, msg->width, msg->height);

	struct tcp_receiver_state* state = (struct tcp_receiver_state*)dom->qemu_receiver.receive_arg;
    
	int shmfd = receive_fd(state->fd);

	if(shmfd == -1) {
	  printf("Failed to receive a file descriptor after MSG_BUFFER\n");
	  return -1;
	}
	    
	dom->qemu_shared_section = (void*)mmap(0, size, PROT_READ, MAP_SHARED, shmfd, 0);
	if(dom->qemu_shared_section == (void*)-1) {
	  printf("mmap failed mapping a shared section\n");
	  dom->qemu_shared_section = 0;
	  close(shmfd);
	  return -1;
	}
	    
	DBGQEMU("Got a new shared section pointer: %p\n", dom->qemu_shared_section);

	close(shmfd);
	
	// If it all worked, set the width/height params and set it all as dirty
	    
	dom->backdrop_width = msg->width;
	dom->backdrop_height = msg->height;
	dom->dirty_x = 0;
	dom->dirty_y = 0;
	dom->dirty_w = msg->width;
	dom->dirty_h = msg->height;
	    
	break;
	    
      }
    case MSG_INIT:
      {
	// Init message: open the semaphore object stated which we will
	// use to synchronise on the shared memory area

	break;
      }
    default:
      {
        printf("Received invalid opcode from Qemu-dm\n");
        return -1;
      }
    }
      
    return 0; // Signal this message is done with
    
  }
}

int notify_controller(struct global_state* state) {

  if(state->control_dom_state != CONTROL_DOM_STATE_INVISIBLE) {
    printf("Ignored request to show control domain when in state %d\n", state->control_dom_state);
    return 0;
  }
  if(!state->domain_control_receiver.receive_arg) {
    printf("No domain controller currently connected\n");
    return 0;
  }
  else {

    struct xen3d_control_message_show* new_message = malloc(sizeof(struct xen3d_control_message_show));

    if(!new_message) {
      printf("OOM notifying the domain controller\n");
      return 0;
    }

    new_message->base.opcode = XEN3D_CONTROL_MESSAGE_SHOW;
    new_message->base.length = sizeof(struct xen3d_control_message_show);

    if(!state->current_domain) {
      new_message->current_domain = -1;
    }
    else {
      new_message->current_domain = state->current_domain->domain;
      if(!state->current_domain->backdrop_texture) {
	new_message->backdrop_width = -1;
	new_message->backdrop_height = -1;
      }
      else {
	new_message->backdrop_width = state->current_domain->backdrop_width;
	new_message->backdrop_height = state->current_domain->backdrop_height;
      }
    }  

    printf("Sending SHOW\n");

    send_or_queue(&state->domain_control_receiver, (char*)new_message, sizeof(struct xen3d_control_message_show));

    printf("Sent; entering PENDING\n");

    state->control_dom_state = CONTROL_DOM_STATE_PENDING;
    /* Suspends input events pending the control domain's appearance */

    return 1;
    
  }

}

int switch_to_domain(struct global_state* state, uint32_t domain) {

  struct domain_list_entry* this_dom = *state->domain_list_head;

  while(this_dom && (this_dom->domain != (int)domain))
    this_dom = this_dom->next;

  if(!this_dom) {
    printf("Couldn't find domain %u\n", domain);
    return 0;
  }
  else {
    state->current_domain = this_dom;
  }

}

int domain_control_process(char* command, int length, void* arg) {

  struct global_state* state = (struct global_state*)arg;

  DBG("Entering domain_control_process: got %d bytes\n", length);

  for(int i = 0; i < length; i++) {
    DBG("%d ", (int)command[i]);
  }

  DBG("\n");

  if(!command)
    return sizeof(struct xen3d_control_message);

  else if(length == sizeof(struct xen3d_control_message)) {

    struct xen3d_control_message* message = (struct xen3d_control_message*)command;
    int new_size = 0;
    
    switch(message->opcode) {
    case XEN3D_CONTROL_MESSAGE_CHOOSE_DOMAIN:
      {
	new_size = sizeof(struct xen3d_control_message_choose_domain);
	break;
      }
    case XEN3D_CONTROL_MESSAGE_READY:
      {
	new_size = sizeof(struct xen3d_control_message);
	break;
      }
    case XEN3D_CONTROL_MESSAGE_SET_CLIP:
      {
	new_size = sizeof(struct xen3d_control_message_set_clip);
	break;
      }
    default:
      {
	printf("Received a control message with an invalid opcode (%u)\n", message->opcode);
      }
    }
    if(!new_size)
      return -1;
    else if(new_size != length)
      return new_size;
    // else, drop through -- we don't need more data right now
  }

  struct xen3d_control_message* complete_message = (struct xen3d_control_message*)command;
  
  switch(complete_message->opcode) {
  case XEN3D_CONTROL_MESSAGE_CHOOSE_DOMAIN:
    {
      struct xen3d_control_message_choose_domain* message = 
	(struct xen3d_control_message_choose_domain*)command;

      if(state->control_dom_state != CONTROL_DOM_STATE_VISIBLE) {
	printf("Got a switch-domain command whilst in state %d; ignored\n", state->control_dom_state);
      }
      
      if(!switch_to_domain(state, message->domain)) {
	printf("Couldn't switch to domain %u\n", message->domain);
      }
      else {
	printf("Switched to domain %u\n", message->domain);
      }
      state->control_dom_state = CONTROL_DOM_STATE_INVISIBLE;
      
      break;
    }
  case XEN3D_CONTROL_MESSAGE_READY:
    {

      printf("Got ready; going to VISIBLE\n");
      if(state->control_dom_state != CONTROL_DOM_STATE_PENDING) {
	printf("Got a 'ready' message from picker whilst in state %d; ignored\n", state->control_dom_state);
      }
      else {
	state->control_dom_state = CONTROL_DOM_STATE_VISIBLE;
	/* Causes input events to be re-enabled and the control domain to be drawn as its clipping request states */
      }
      break;
    }
  case XEN3D_CONTROL_MESSAGE_SET_CLIP:
    {
      struct xen3d_control_message_set_clip* message =
	(struct xen3d_control_message_set_clip*)command;

      printf("Got SET_CLIP: offset (%d, %d) with %d rects\n", message->offset_x, message->offset_y, message->nrects);

      int required_size = sizeof(struct xen3d_control_message_set_clip)
	+ (message->nrects * sizeof(struct xen3d_clip_rect));

      if(length != required_size)
	return required_size;

      // If we get to here, we have a complete message including the clip-rectangles
      state->control_dom_off_x = message->offset_x;
      state->control_dom_off_y = message->offset_y;

      if(state->control_dom_clip_rects) {
	free(state->control_dom_clip_rects);
	state->control_dom_n_clip_rects = 0;
	state->control_dom_clip_rects = 0;
      }
      
      state->control_dom_clip_rects = malloc(sizeof(struct xen3d_clip_rect) * message->nrects);
      if(!state->control_dom_clip_rects) {
	printf("Failed to malloc for %d clip-rectangles (%d bytes); control domain will be invisible\n", message->nrects, message->nrects * sizeof(struct xen3d_clip_rect));
      }
      else {
	state->control_dom_n_clip_rects = message->nrects;
	memcpy((void*)state->control_dom_clip_rects,
	       (void*)(((char*)message) + sizeof(struct xen3d_control_message_set_clip)),
	       (sizeof(struct xen3d_clip_rect) * message->nrects));
      }

      break;
    }
  default:
    {
      printf("Received a control message with an invalid opcode (%u)\n", ((struct xen3d_control_message*)command)->opcode);
    }
  }

  return 0; // Command is finished with
  
}

int x_process(char* command, int length, void* arg) {

  if(!command) {
    // Initially need at least this:
    return sizeof(XVMGLWindowingCommand);
  }
  else {
    XVMGLWindowingCommand* message = (XVMGLWindowingCommand*)command;
    int ncliprects = ntohl(message->length);
	
    int target_length = sizeof(XVMGLWindowingCommand) + (ncliprects * sizeof(BoxRec));

    DBG("Target length is %d!\n", target_length);
	
    if(length != target_length) {
      return target_length;
    }
    else {
	
      struct domain_list_entry* domain = (struct domain_list_entry*)arg;
	    
      XVMGLWindowingCommand* message = (XVMGLWindowingCommand*)command;

      // Command is ready for processing
      uint32_t screen = ntohl(message->screenid);
      uint32_t window = ntohl(message->glWindow);

      // Find the appropriate drawable surface
	    
      DBG("Searching the list for screen %u, window %u\n", screen, window);
		    
      struct drawable_surface_list** this_surface = &domain->drawables;
		    
      while((*this_surface) && ((*this_surface)->screen != screen || (*this_surface)->window != window)) {
	this_surface = &((*this_surface)->next);
      }
			
      if(!*this_surface) {
	// Create a new drawable surface -- we haven't heard any updates regarding
	// this one yet, but presumably they are coming.
	DBG("Didn't find this drawable\n");
	(*this_surface) = malloc(sizeof(struct drawable_surface_list));
	memset(*this_surface, 0, sizeof(struct drawable_surface_list));
	(*this_surface)->window = window;
	(*this_surface)->screen = screen;
      }
	    
      struct drawable_surface_list* to_update = *this_surface;

      DBG("Window update for screen %u / window %u\n", screen, window);

      to_update->x = ntohl(message->x);
      to_update->y = ntohl(message->y);
      to_update->w = ntohl(message->width);
      to_update->h = ntohl(message->height);
      DBG("Got a window update:\n");
      DBG("Screen-ID: %u\n", screen);
      DBG("GL Window: %u\n", window);
      DBG("Location: (%u, %u)\n", to_update->x, to_update->y);
      DBG("Dimension: %ux%u\n", to_update->w, to_update->h);

      to_update->ncliprects = ntohl(message->length);

      DBG("Cliprects: %d\n", to_update->ncliprects);
		    
      if(to_update->cliprects)
	free(to_update->cliprects);
		
      to_update->cliprects = malloc(to_update->ncliprects * sizeof(BoxRec));
		    
      memcpy(to_update->cliprects, 
	     ((char*)message) + sizeof(XVMGLWindowingCommand),
	     sizeof(BoxRec) * to_update->ncliprects);

      int i;
      for(i = 0; i < to_update->ncliprects; i++) {

	BoxPtr thisbox = &(((BoxPtr)(((char*)message) + sizeof(XVMGLWindowingCommand)))[i]);

	DBG("Rect %d: X %hu to %hu, Y %hu to %hu\n", i, thisbox->x1, thisbox->x2, thisbox->y1, thisbox->y2);

      }
	    
      // Command is finished with
	    
      return 0;
	
    }
  }
}

int gallium_client_process(char* message, int length, void* arg) {

  DBG("Client seems to have reached its target (was %d bytes)\n", length);

  if(!message)
    return sizeof(struct message_header);

  if(length == sizeof(struct message_header)) {
    DBG("Target was sizeof(struct message_header)\n");
    int message_size = get_message_size(message);
    DBG("The complete message appears to be %d bytes long\n", message_size);

    DBG("Gallium: asking for %d bytes\n", message_size);
    if(message_size < ((int)sizeof(struct message_header))) {
      printf("Error: get_message_size returned less than the minimum message length\n");
      printf("The opcode was: %u\n", ((struct message_header*)message)->opcode);
    }

    if(message_size != length)
      return message_size;
    // else drop through to full message processing
  }

  DBG("Dispatching a message...\n");
  
  struct client_list_entry* client = (struct client_list_entry*)arg;

  int success = dispatch_message(client, message);

  if(!success) {
    DBG("Dispatch was unsuccessful: returning error\n");
    return -1;
  }
  else {
    return 0; // Mark message dealt with
  }

}

struct domain_list_entry* find_or_create_domain(struct domain_list_entry** list, int dom, struct global_state* global_state) {

  struct domain_list_entry* next = *list;
  
  while(next) {
    if(next->domain == dom)
      return next;
    next = next->next;
  }    

  // No existing entry: create one
  struct domain_list_entry* domain_entry = (struct domain_list_entry*) malloc(sizeof(struct domain_list_entry));
  // Set up state common to both X extension and Qemu connections
  domain_entry->next = 0;
  domain_entry->domain = dom;
  domain_entry->global_state = global_state;
  domain_entry->clients = 0;
  domain_entry->drawables = 0;
  domain_entry->qemu_sem_handle = -1;
  domain_entry->qemu_shared_section = 0;
  domain_entry->backdrop_texture = 0;
  // No need to set the dirty rectangle; it gets set whenever the shared section
  // changes in any case.

  // The validity of a receive or send function pointer hinges on its argument
  // being non-null.
  domain_entry->x_receiver.receive_arg = 0;
  domain_entry->qemu_receiver.receive_arg = 0;
  domain_entry->x_receiver.send_arg = 0;
  domain_entry->qemu_receiver.send_arg = 0;

  if(dom == global_state->trusted_domain) {
    global_state->trusted_domain_descriptor = domain_entry;
  }
    
  while(*list)
    list = &((*list)->next);
  *list = domain_entry;

}

void handle_control_connection(struct xidc_receiver_state* xidc_data, struct global_state* global_state) {

  global_state->domain_control_receiver.command = malloc(sizeof(struct xen3d_control_message));
  global_state->domain_control_receiver.recv_current = global_state->domain_control_receiver.command;
  global_state->domain_control_receiver.target = global_state->domain_control_receiver.command + sizeof(struct xen3d_control_message);
  global_state->domain_control_receiver.send = xidc_send;
  global_state->domain_control_receiver.send_arg = (void*)xidc_data;
  global_state->domain_control_receiver.receive = xidc_receive;
  global_state->domain_control_receiver.receive_arg = (void*)xidc_data;
  global_state->domain_control_receiver.process = domain_control_process;
  global_state->domain_control_receiver.process_arg = (void*)global_state;
  global_state->domain_control_receiver.max_updates_before_yield = 5;
  global_state->domain_control_receiver.partial_send = 0;  

}

void handle_new_connection(int domid, int clientid, struct xidc_receiver_state* xidc_data, struct domain_list_entry** list, struct global_state* global_state) {

  DBG("Got a new 3D client connection from domain %d\n", domid);
  
  struct domain_list_entry* domain_entry = find_or_create_domain(list, domid, global_state);

  struct client_list_entry* new_client = malloc(sizeof(struct client_list_entry));

  new_client->next = 0;
  new_client->receiver.command = malloc(sizeof(struct message_header));
  new_client->receiver.recv_current = new_client->receiver.command;
  new_client->receiver.target = new_client->receiver.command + sizeof(struct message_header);
  new_client->receiver.send = xidc_send;
  new_client->receiver.send_arg = (void*)xidc_data;
  new_client->receiver.receive = xidc_receive;
  new_client->receiver.receive_arg = (void*)xidc_data;
  new_client->receiver.process = gallium_client_process;
  new_client->receiver.process_arg = (void*)new_client;
  new_client->receiver.max_updates_before_yield = MAX_GALLIUM_CMDS_BEFORE_YIELD;
  new_client->receiver.partial_send = 0;

  new_client->domain = domain_entry;
  new_client->clientid = clientid;
  new_client->global_state = global_state;
  new_client->screen = 0;

  struct client_list_entry** next_client = &(domain_entry->clients);

  while(*next_client) {
    next_client = &((*next_client)->next);
  }

  *next_client = new_client;

}

void handle_new_xext_connection(int domid, struct xidc_receiver_state* xidc_data, struct domain_list_entry** list, struct global_state* global_state) {

  DBG("Handling new X extension connection from domain %d\n", domid);

  printf("New X extension connection from domain %d, using event channel %d\n",domid, xidc_data->evtid);
  
  struct domain_list_entry* domain_entry = find_or_create_domain(list, domid, global_state);

  domain_entry->x_receiver.command = malloc(sizeof(XVMGLWindowingCommand));
  domain_entry->x_receiver.recv_current = domain_entry->x_receiver.command;
  domain_entry->x_receiver.target = domain_entry->x_receiver.command + sizeof(XVMGLWindowingCommand);
  domain_entry->x_receiver.receive = xidc_receive;
  domain_entry->x_receiver.receive_arg = (void*)xidc_data;
  domain_entry->x_receiver.send = 0;
  domain_entry->x_receiver.send_arg = 0;
  // The X extension traffic is entirely one-way
  domain_entry->x_receiver.process = x_process;
  domain_entry->x_receiver.process_arg = (void*)domain_entry;
  domain_entry->x_receiver.max_updates_before_yield = MAX_X_CMDS_BEFORE_YIELD;
  
}

void handle_new_qemu_connection(struct domain_list_entry** list, int listenfd, struct global_state* global_state) {

  struct sockaddr_un client_address;
  socklen_t length = sizeof(struct sockaddr_un);
  memset(&client_address, 0, sizeof(struct sockaddr_un));

  DBG("Handling new Qemu extension connection\n");

  int connfd = accept(listenfd, (struct sockaddr*)&client_address, &length);

  DBG("Got a new connection from a Qemu-dm: fd is %d\n", connfd);
  
  if(connfd == -1) {
    printf("accept() failed\n");
    return;
  }

  printf("New Qemu extension connection from %s, using fd %d\n", client_address.sun_path, connfd);

  struct init_message initdata;
  int bytesreceived = 0;

  while(bytesreceived < sizeof(struct init_message)) {

    int ret = recv(connfd, ((char*)&initdata) + bytesreceived, sizeof(struct init_message) - bytesreceived, 0);

    if(ret <= 0) {
      printf("Failed to receive an init message from the new Qemu (return %d, errno %d\n", ret, errno);
      return;
    }

    bytesreceived += ret;

  }

  DBGQEMU("Got an init message\n");
  printf("The new Qemu represents domain %d\n", initdata.domain);

  struct domain_list_entry* dom = find_or_create_domain(list, initdata.domain, global_state);

  dom->qemu_sem_handle = semget(initdata.sem_key, 1, 0);
	    
  if(dom->qemu_sem_handle == -1) {
    printf("Couldn't open semaphore supplied by qemu-dm; destroying\n");
    destroy_qemu(dom, list, global_state);
    return;
  }
  DBGQEMU("Successfully opened remote semaphore\n");

  setnonblocking(connfd);
    
  // Set up state applicable only to the Qemu-dm  
  struct tcp_receiver_state* state = (struct tcp_receiver_state*)malloc(sizeof(struct tcp_receiver_state));

  state->fd = connfd;

  dom->qemu_receiver.command = malloc(sizeof(int));
  dom->qemu_receiver.recv_current = dom->qemu_receiver.command;
  dom->qemu_receiver.target = dom->qemu_receiver.command + sizeof(int);
  dom->qemu_receiver.receive = tcp_receive;
  dom->qemu_receiver.receive_arg = (void*)state;
  dom->qemu_receiver.send_blocking = tcp_send_blocking;
  dom->qemu_receiver.send_arg = (void*)state;
  dom->qemu_receiver.process = qemu_process;
  dom->qemu_receiver.process_arg = (void*)dom;
  dom->qemu_receiver.max_updates_before_yield = MAX_QEMU_CMDS_BEFORE_YIELD;

  struct display_state_message* outmsg = (struct display_state_message*)malloc(sizeof(struct display_state_message));
	    
  outmsg->opcode = MSG_DISPLAY_STATE;
  outmsg->depth = dom->global_state->window_depth;
	    
  if(dom->qemu_receiver.send_blocking(dom->qemu_receiver.send_arg, (char*)outmsg, sizeof(struct display_state_message))) {
    printf("Successfully initialised 2D display for domain %d\n", dom->domain);
  }
  else {
    printf("Failed to send window depth to Qemu\n");
    destroy_qemu(dom, list, global_state);
  }
 
}

void destroy_blend(struct client_blend* blend, struct pipe_context* pipe) {

  DBGDE("Destroying blend %p\n", blend);

  pipe->delete_blend_state(pipe, blend->base);
  free(blend);

}

void destroy_query(struct client_query* query, struct pipe_context* pipe) {

  DBGDE("Destroying query %p\n", query);

  pipe->destroy_query(pipe, query->base);
  free(query);

}

void destroy_sampler(struct client_sampler* sam, struct pipe_context* pipe) {

  DBGDE("Destroying sampler %p\n", sam);

  pipe->delete_sampler_state(pipe, sam->base);
  free(sam);

}

void destroy_rast(struct client_rast* rast, struct pipe_context* pipe) {

  DBGDE("Destroying rasterizer %p\n", rast);

  pipe->delete_rasterizer_state(pipe, rast->base);
  free(rast);

}

void destroy_dsa(struct client_dsa* dsa, struct pipe_context* pipe) {

  DBGDE("Destroying DSA %p\n", dsa);

  pipe->delete_depth_stencil_alpha_state(pipe, dsa->base);
  free(dsa);

}

void destroy_vs(struct client_vs* vs, struct pipe_context* pipe) {

  DBGDE("Destroying vertex shader %p\n", vs);

  pipe->delete_vs_state(pipe, vs->base);
  free(vs);

}

void destroy_fs(struct client_fs* fs, struct pipe_context* pipe) {

  DBGDE("Destroying fragment shader %p\n", fs);

  pipe->delete_fs_state(pipe, fs->base);
  free(fs);

}

void destroy_context(struct client_context* ctx) {

#ifdef CS_DEBUG_DESTROY
#undef DBG_DUMP
#define DBG_DUMP(typename, map) map_dump_##typename(map)
#endif

  DBGDE("Map states before destruction:\n");
  DBGDE("Blends:\n");
  DBG_DUMP(blendmap, ctx->blends);
  DBGDE("Queries:\n");
  DBG_DUMP(querymap, ctx->queries);
  DBGDE("Samplers:\n");
  DBG_DUMP(samplermap, ctx->samplers);
  DBGDE("Rasterizers:\n");
  DBG_DUMP(rastmap, ctx->rasts);
  DBGDE("Depth/stencil/alpha states:\n");
  DBG_DUMP(dsamap, ctx->dsas);
  DBGDE("Vertex shaders:\n");
  DBG_DUMP(vsmap, ctx->vss);
  DBGDE("Fragment shaders:\n");
  DBG_DUMP(fsmap, ctx->fss);

#ifdef CS_DEBUG_DESTROY
#undef DBG_DUMP
#ifdef CS_DEBUG_MAPS
#define DBG_DUMP(typename, map) map_dump_##typename(map)
#else
#define DBG_DUMP(typename, map)
#endif
#endif

  MAP_FOR_EACH_VALUE(struct client_blend*, ctx->blends, blendmap, destroy_blend, ctx->pipe_ctx);
  MAP_FOR_EACH_VALUE(struct client_query*, ctx->queries, querymap, destroy_query, ctx->pipe_ctx);
  MAP_FOR_EACH_VALUE(struct client_sampler*, ctx->samplers, samplermap, destroy_sampler, ctx->pipe_ctx);
  MAP_FOR_EACH_VALUE(struct client_rast*, ctx->rasts, rastmap, destroy_rast, ctx->pipe_ctx);
  MAP_FOR_EACH_VALUE(struct client_dsa*, ctx->dsas, dsamap, destroy_dsa, ctx->pipe_ctx);
  MAP_FOR_EACH_VALUE(struct client_vs*, ctx->vss, vsmap, destroy_vs, ctx->pipe_ctx);
  MAP_FOR_EACH_VALUE(struct client_fs*, ctx->fss, fsmap, destroy_fs, ctx->pipe_ctx);

  MAP_DESTROY(blendmap, ctx->blends);
  MAP_DESTROY(querymap, ctx->queries);
  MAP_DESTROY(samplermap, ctx->samplers);
  MAP_DESTROY(rastmap, ctx->rasts);
  MAP_DESTROY(dsamap, ctx->dsas);
  MAP_DESTROY(vsmap, ctx->vss);
  MAP_DESTROY(fsmap, ctx->fss);

  ctx->pipe_ctx->destroy(ctx->pipe_ctx);

}

void destroy_client_buffer(struct client_buffer* buf, struct pipe_screen* screen) {

  DBGDE("Destroying buffer %p\n", buf); 

  pipe_buffer_reference(screen, &buf->base, NULL);
  free(buf);

}

void destroy_client_texture(struct client_texture* tex, struct pipe_screen* screen, struct client_list_entry* client) {

  DBGDE("Destroying texture %p\n", tex);

  pipe_texture_reference(&tex->base, NULL);

  if(tex->frontbuffer) {
    
    struct drawable_surface_list** list = &client->domain->drawables;

    while(*list) {
      if((*list)->texture == tex) {
	struct drawable_surface_list* li_to_delete = *list;
	*list = (*list)->next;
	free(li_to_delete);
	break;
      }
      list = &((*list)->next);
    }

    pipe_texture_reference(&tex->frontbuffer, NULL);

  }

  free(tex);

}

void destroy_client_surface(struct client_surface* surf, struct pipe_screen* screen) {

  DBGDE("Destroying surface %p\n", surf);

  pipe_surface_reference(&surf->base, NULL);

  free(surf);

}

void destroy_client_context(struct client_context* ctx) {

  DBGDE("Destroying context %p\n", ctx);

  destroy_context(ctx);

  free(ctx);

}

void destroy_tcp(struct tcp_receiver_state* state) {

  if(state->fd != -1)
    close(state->fd);
  free(state);

}

void destroy_xidc(struct xidc_receiver_state* state) {

  if(state->buffer)
    xc_gnttab_munmap(state->gntdev_handle, state->buffer, 4);
    
  if(state->return_buffer)
    xc_gnttab_munmap(state->return_gntdev_handle, state->return_buffer, 4);
    
  if(state->gntdev_handle != -1)
    xc_gnttab_close(state->gntdev_handle);

  if(state->return_gntdev_handle != -1)
    xc_gnttab_close(state->return_gntdev_handle);

  if(state->evtid != -1)
    xc_evtchn_unbind(state->evtchnhandle, state->evtid);

  if(state->return_evtid != -1)
    xc_evtchn_unbind(state->evtchnhandle, state->return_evtid);

  free(state);

}

void destroy_domain(struct domain_list_entry* domain, struct domain_list_entry** list, struct global_state* global_state) {

  if(global_state->trusted_domain_descriptor == domain) {
    printf("Warning: the trusted domain is dying\n");
    global_state->trusted_domain_descriptor = 0;
  }

  while(*list) {

    if(*list == domain) {
      struct domain_list_entry* to_free = *list;
      *list = domain->next;
      if(to_free == global_state->current_domain)
	global_state->current_domain = 0;
      free(to_free);
      break;
    }
    
    list = &((*list)->next);

  }
  
}

int destroy_xserver(struct domain_list_entry* domain, struct domain_list_entry** list, struct global_state* global_state) {

  if(domain->x_receiver.receive_arg != 0) {
    destroy_xidc((struct xidc_receiver_state*)domain->x_receiver.receive_arg);
    domain->x_receiver.receive_arg = 0;
    domain->x_receiver.receive = 0;
  }
  
  if(domain->x_receiver.command != 0) {
    free(domain->x_receiver.command);
    domain->x_receiver.command = 0;
  }
  
  struct drawable_surface_list* drawable = domain->drawables;
  
  while(drawable) {
  
    if(drawable->cliprects) {
      free(drawable->cliprects);
      drawable->cliprects = 0;
      drawable->ncliprects = 0;
    }
    
    drawable = drawable->next;
    
  }    

  if((!domain->clients) && (!domain->qemu_receiver.receive_arg)) {
    destroy_domain(domain, list, global_state);  
    return 1;
  }
  else {
    return 0;
  }
 
}

int destroy_qemu(struct domain_list_entry* domain, struct domain_list_entry** list, struct global_state* global_state) {

  if(domain->qemu_receiver.receive_arg) {
    destroy_tcp((struct tcp_receiver_state*)domain->qemu_receiver.receive_arg);
    domain->qemu_receiver.receive_arg = 0;
    domain->qemu_receiver.receive = 0;
  }
  
  if(domain->qemu_receiver.command) {
    free(domain->qemu_receiver.command);
    domain->qemu_receiver.command = 0;
  }
  
  if(domain->qemu_shared_section) {
    munmap(domain->qemu_shared_section, domain->backdrop_width * domain->backdrop_height * (domain->global_state->window_pixel_size / 8));
    domain->qemu_shared_section = 0;
  }
    
  if(domain->qemu_sem_handle != -1) {
    semctl(domain->qemu_sem_handle, IPC_RMID, 0);
    domain->qemu_sem_handle = -1;
  }
  
  pipe_texture_reference(&(domain->backdrop_texture), NULL);
    
  if((!domain->clients) && (!domain->x_receiver.receive_arg)) {
    destroy_domain(domain, list, global_state);
    return 1;
  }
  else {
    return 0;
  }

}

int destroy_client(struct client_list_entry* client, struct client_list_entry** list, struct domain_list_entry* domain, struct domain_list_entry** domlist, struct global_state* global_state) {

  DBG("Destroying client\n");

  destroy_xidc((struct xidc_receiver_state*)client->receiver.receive_arg);
  
#ifdef CS_DEBUG_DESTROY
#undef DBG_DUMP
#define DBG_DUMP(typename, map) map_dump_##typename(map)
#endif

  DBGDE("Map states before destruction:\n");
  DBGDE("Buffers:\n");
  DBG_DUMP(buffermap, client->screen->buffers);
  DBGDE("Surfaces:\n");
  DBG_DUMP(surfacemap, client->screen->surfaces);
  DBGDE("Textures:\n");
  DBG_DUMP(texturemap, client->screen->textures);
  DBGDE("Contexts:\n");
  DBG_DUMP(contextmap, client->screen->contexts);

#ifdef CS_DEBUG_DESTROY
#undef DBG_DUMP
#ifdef CS_DEBUG_MAPS
#define DBG_DUMP(typename, map) map_dump_##typename(map)
#else
#define DBG_DUMP(typename, map)
#endif
#endif

  // Now destroy all state objects registered to this client

  MAP_FOR_EACH_VALUE(struct client_buffer*, client->screen->buffers, buffermap, 
		     destroy_client_buffer, client->screen->screen);
  MAP_FOR_EACH_VALUE(struct client_surface*, client->screen->surfaces, surfacemap, 
		     destroy_client_surface, client->screen->screen);
  MAP_FOR_EACH_VALUE(struct client_texture*, client->screen->textures, texturemap, 
		     destroy_client_texture, client->screen->screen, client);
  MAP_FOR_EACH_VALUE(struct client_context*, client->screen->contexts, contextmap, destroy_client_context);

  MAP_DESTROY(buffermap, client->screen->buffers);
  MAP_DESTROY(surfacemap, client->screen->surfaces);
  MAP_DESTROY(texturemap, client->screen->textures);
  MAP_DESTROY(contextmap, client->screen->contexts);

  while(*list) {

    if(*list == client) {
      struct client_list_entry* to_free = *list;
      *list = client->next;
      free(to_free);
      break;
    }
    list = &((*list)->next);

  }
  
  if((!domain->qemu_receiver.receive_arg) && (!domain->x_receiver.receive_arg) && (!domain->clients)) {
    destroy_domain(domain, domlist, global_state);
    return 1;
  }
  else {
    return 0;
  }
  
}

int handle_xenstore_event(struct xs_handle* xsh, int evtchnhandle, struct domain_list_entry** domlisthead, struct global_state* global_state) {

  int len;
  char** watch_paths = xs_read_watch(xsh, &len);
  if(!watch_paths) {
    printf("Failed to get watch paths\n");
    return 1;
  }

  for(int i = 0; i < len; i += 2) {

    char* path = watch_paths[i];
    char* token = watch_paths[i+1];
    printf("%s changed (token %s)\n", path, token);

    if(strncmp(token, "watch", 5) != 0) {

      int domid;
      int clientid;
      int destroytask = 0;

      int len;

      if(xs_read(xsh, XBT_NULL, path, &len)) {
	printf("Ignored non-destructive change to %s (token %s)\n", path, token);
	continue;
      }

      if(sscanf(token, "%d %d", &domid, &clientid) != 2) {
	if(sscanf(token, "%d X", &domid) != 1) {
	  if(strcmp(token, "control")) {
	    printf("Threw away a strange token: %s\n", token);
	    continue;
	  }
	  else {
	    destroytask = 2;
	  }
	}
	else {
	  destroytask = 1;
	}
      }

      if(!xs_unwatch(xsh, path, token)) {
	printf("Failed to unwatch %s: no watch on this path\n", path);
      }

      if(destroytask == 1) {
	printf("Destruction of domain %d's X server signalled\n", domid);
      }
      else if(destroytask == 2) {
	printf("Destruction of the domain controller signalled\n");
      }
      else {
	printf("Destruction of domain %d / client %d signalled\n", domid, clientid);
      }

      if(destroytask == 2) {
	destroy_xidc(global_state->domain_control_receiver.receive_arg);
	global_state->domain_control_receiver.receive_arg = 0;
	if(global_state->domain_control_receiver.partial_send) {
	  free(global_state->domain_control_receiver.partial_send);
	  global_state->domain_control_receiver.partial_send = 0;
	}
	global_state->control_dom_state = CONTROL_DOM_STATE_INVISIBLE;
	global_state->control_dom_off_x = 0;
	global_state->control_dom_off_y = 0;
	global_state->control_dom_n_clip_rects = 0;
	if(global_state->control_dom_clip_rects)
	  free(global_state->control_dom_clip_rects);
	global_state->control_dom_clip_rects = 0;
      }
      else {

      struct domain_list_entry* todestroy = *domlisthead;

      while(todestroy && (todestroy->domain != domid))
	todestroy = todestroy->next;

      if(!todestroy) {
	printf("Couldn't find a domain entry for %d!\n", domid);
	continue;
      }

      if(destroytask == 1) {
	destroy_xserver(todestroy, domlisthead, global_state);
      }
      else {
	struct client_list_entry* clienttodestroy = todestroy->clients;
	while(clienttodestroy && (clienttodestroy->clientid != clientid))
	  clienttodestroy = clienttodestroy->next;

	if(!clienttodestroy) {
	  printf("Couldn't find domain %d / client %d to destroy!\n", domid, clientid);
	  printf("Clients for domain %d:\n", todestroy->domain);
	  clienttodestroy = todestroy->clients;
	  while(clienttodestroy) {
	    printf("ID %d\n", clienttodestroy->clientid);
	    clienttodestroy = clienttodestroy->next;
	  }
	  
	  continue;
	}
	else {
	  destroy_client(clienttodestroy, &(todestroy->clients), todestroy, domlisthead, global_state);
	}
      }
      }
    
    }
    else  {

    int domid;
    int clientid;

    char test = '\0';

    sscanf(path, WATCH_NODE"/%d/%d/fronten%c", &domid, &clientid, &test);

    if(domid >= 0 && clientid >= 0 && test == 'd') {

      char* frontend = xs_read(xsh, XBT_NULL, watch_paths[i], NULL);
      if(!frontend) {
	printf("Failed to read from %s\n", watch_paths[i]);
	continue;
      }
      else {

	xs_rm(xsh, XBT_NULL, path);

	struct xidc_receiver_state* arg = malloc(sizeof(struct xidc_receiver_state));
	if(!arg) {
	  printf("Couldn't malloc receiver data struct\n");
	  continue;
	}

	int client_type = 0;
	char evtchnpath[1024];

	sprintf(evtchnpath, "/local/domain/%d/%s/type", domid, frontend);
	char* typestring = xs_read(xsh, XBT_NULL, evtchnpath, NULL);
	if(!typestring) {
	  printf("Failed to read from %s\n", evtchnpath);
	  free(arg);
	  continue;
	}
	sscanf(typestring, "%d", &client_type);

	free(typestring);

	if(client_type == 2) { // The domain controller
	  if(domid != global_state->trusted_domain) {
	    printf("Permission denied: domain %d cannot become the controller\n", domid);
	    free(arg);
	    continue;
	  }
	}

	arg->evtchnhandle = evtchnhandle;

	printf("Frontend discovered for domain %d / client %d, giving its relative path as %s\n", domid, clientid, frontend);

	sprintf(evtchnpath, "/local/domain/%d/%s/event-channel", domid, frontend);
	char* evtstring = xs_read(xsh, XBT_NULL, evtchnpath, NULL);
	if(!evtstring) {
	  printf("Failed to read from %s\n", evtchnpath);
	  free(arg);
	  continue;
	}
	evtchn_port_or_error_t remoteport;
	sscanf(evtstring, "%d", &remoteport);

	arg->evtid = xc_evtchn_bind_interdomain(evtchnhandle, domid, remoteport);

	if(arg->evtid == -1) {
	  printf("Failed to bind domain %d / event port %d\n", domid, remoteport);
	  free(arg);
	  continue;
	}

	printf("Remote specifies an event channel with ID %d\n", remoteport);

	free(evtstring);

	for(i=0; i < 4; i++) {
	  sprintf(evtchnpath, "/local/domain/%d/%s/pages/%d", domid, frontend, i);
	  evtstring = xs_read(xsh, XBT_NULL, evtchnpath, NULL);
	  if(!evtstring) {
	    printf("Failed to read from %s\n", evtchnpath);
	    free(arg);
	    continue;
	  }
	  sscanf(evtstring, "%u", &(arg->grants[i]));
	  printf("Remote specifies incoming ring page %d has grant %u\n", i, arg->grants[i]);
	  free(evtstring);
	}
	char remotewatch[1024];
	sprintf(remotewatch, "/local/domain/%d/%s", domid, frontend);
	char watchtoken[50];
	if(client_type == 1) // An X extension
	  sprintf(watchtoken, "%d X", domid);
	else if(client_type == 2) // The domain controller
	  sprintf(watchtoken, "control");
	else
	  sprintf(watchtoken, "%d %d", domid, clientid);

	if(!xs_watch(xsh, remotewatch, watchtoken)) {
	  printf("Unable to watch %s with token %s; dropping\n", remotewatch, watchtoken);
	  free(arg);
	  continue;
	}

	arg->gntdev_handle = xc_gnttab_open();

	if(arg->gntdev_handle == -1) {
	  printf("Couldn't open gntdev\n");
	  free(arg);
	  continue;
	}
	
	uint32_t domains[4];
	for(int i = 0; i < 4; i++)
	  domains[i] = domid;

	arg->buffer = xc_gnttab_map_grant_refs(arg->gntdev_handle, 4, domains, arg->grants, PROT_READ | PROT_WRITE);

	if(!arg->buffer) {
	  printf("Couldn't map the supplied grants\n");
	  free(arg);
	  continue;
	}

	if(client_type == 1) { // an X extension
	  // No return ringbuffer is required; X traffic is one-way
	  arg->return_gntdev_handle = -1;
	  arg->return_buffer = 0;
	  handle_new_xext_connection(domid, arg, domlisthead, global_state);
	}
	else {
	  // Get the event channel and ring buffer for return traffic
	  sprintf(evtchnpath, "/local/domain/%d/%s/return-event-channel", domid, frontend);
	  evtstring = xs_read(xsh, XBT_NULL, evtchnpath, NULL);
	  if(!evtstring) {
	    printf("Failed to read from %s\n", evtchnpath);
	    free(arg);
	    continue;
	  }

	  evtchn_port_or_error_t remoteport;
	  sscanf(evtstring, "%d", &remoteport);

	  arg->return_evtid = xc_evtchn_bind_interdomain(evtchnhandle, domid, remoteport);

	  if(arg->return_evtid == -1) {
	    printf("Failed to bind domain %d / event port %d\n", domid, remoteport);
	    free(arg);
	    continue;
	  }

	  printf("Remote specifies a return event channel with ID %d\n", remoteport);
	  
	  free(evtstring);

	  for(i=0; i < 4; i++) {
	    sprintf(evtchnpath, "/local/domain/%d/%s/return-pages/%d", domid, frontend, i);
	    evtstring = xs_read(xsh, XBT_NULL, evtchnpath, NULL);
	    if(!evtstring) {
	      printf("Failed to read from %s\n", evtchnpath);
	      free(arg);
	      continue;
	    }
	    sscanf(evtstring, "%u", &(arg->return_grants[i]));
	    printf("Remote specifies outgoing ring page %d has grant %u\n", i, arg->return_grants[i]);
	    free(evtstring);
	  }
	  arg->return_gntdev_handle = xc_gnttab_open();

	  if(arg->return_gntdev_handle == -1) {
	    printf("Couldn't open gntdev\n");
	    free(arg);
	    continue;
	  }
	  
	  arg->return_buffer = xc_gnttab_map_grant_refs(arg->return_gntdev_handle, 4, domains, arg->return_grants, PROT_READ | PROT_WRITE);
	  
	  if(!arg->return_buffer) {
	    printf("Couldn't map the supplied grants (return channel)\n");
	    free(arg);
	    continue;
	  }

	  if(client_type == 0)
	    handle_new_connection(domid, clientid, arg, domlisthead, global_state);
	  else if(client_type == 2)
	    handle_control_connection(arg, global_state);
	}
      }
    }
    }
  }
}

uint32_t ring_write_space(uint32_t readoff, uint32_t writeoff, uint32_t bufsize) {

  if(readoff > writeoff)
    return (readoff - writeoff) - 1;
  else
    return (bufsize - (writeoff - readoff)) - 1;

}

// Continue I/O to all clients using XIDC as their transport. X extensions are
// simplex, and so easy to deal with. Gallium clients are full duplex; however
// all remote receives are blocking, so we must complete a partial send before
// there will be anything to receive.

// The return value indicates whether we're expecting an event before any more
// work can be done; return 1 means that work remains to be done and we've yielded
// to be friendly to other system components,
// return 0 means nothing remains and an event should be awaited.
// return -1 means a fundamental error which applies to the system globally,
// not just a single client.

int handle_xen_event(struct domain_list_entry** domain_list_head_p, int evtchnhandle, int evtchnfd, struct global_state* global_state) {

  struct domain_list_entry* next_domain = *domain_list_head_p;

  int work_remaining = 0;

  // First catch and clear the event(s)
  fd_set selectset;
  struct timeval dontwait;
  dontwait.tv_sec = 0;
  dontwait.tv_usec = 0;
  
  FD_ZERO(&selectset);
  FD_SET(evtchnfd, &selectset);
  while(select(evtchnfd + 1, &selectset, 0, 0, &dontwait) > 0) {
    // At least one event is waiting
    evtchn_port_or_error_t waiting = xc_evtchn_pending(evtchnhandle);
    if(waiting == -1) {
      printf("Failed reading the event channel device\n");
      return -1;
    }
    if(xc_evtchn_unmask(evtchnhandle, waiting) == -1) {
      printf("Failed to unmask an event channel\n");
      return -1;
    }
    FD_ZERO(&selectset);
    FD_SET(evtchnfd, &selectset);
  }

  // All events are cleared
  // Now handle everything which is readable and writable, such that we can
  // safely select in the knowledge that if there were anything to do, an event
  // has been raised and we will find out.

  if(global_state->domain_control_receiver.receive_arg) {

    struct xidc_receiver_state* state = (struct xidc_receiver_state*)global_state->domain_control_receiver.receive_arg;

    uint32_t send_bufsize = ((uint32_t*)state->return_buffer)[0];
    uint32_t send_readoff = ((uint32_t*)state->return_buffer)[1];
    uint32_t send_writeoff = ((uint32_t*)state->return_buffer)[2];
    
    uint32_t readoff = ((uint32_t*)state->buffer)[1];
    uint32_t writeoff = ((uint32_t*)state->buffer)[2];
      
    if(global_state->domain_control_receiver.partial_send && ring_write_space(send_readoff, send_writeoff, send_bufsize)) {

      int sent = global_state->domain_control_receiver.send(global_state->domain_control_receiver.send_arg,
							    global_state->domain_control_receiver.partial_send +
							    global_state->domain_control_receiver.bytes_sent,
							    global_state->domain_control_receiver.partial_send_length - 
							    global_state->domain_control_receiver.bytes_sent);


      global_state->domain_control_receiver.bytes_sent += sent;
      if(global_state->domain_control_receiver.bytes_sent == global_state->domain_control_receiver.partial_send_length) {
	free(global_state->domain_control_receiver.partial_send);
	global_state->domain_control_receiver.partial_send = 0;
      }

    xc_evtchn_notify(evtchnhandle, state->return_evtid);

    }
	
    if((!global_state->domain_control_receiver.partial_send) && readoff != writeoff) {      
	
      int ret = receiver_process_data(&(global_state->domain_control_receiver));
      if(ret == -1)
	work_remaining = 1;
	
    }

  }

  while(next_domain) {
    
    struct domain_list_entry* next_next_domain = next_domain->next;

    if(next_domain->x_receiver.receive_arg) {

      struct xidc_receiver_state* state = (struct xidc_receiver_state*)next_domain->x_receiver.receive_arg;

      uint32_t readoff = ((uint32_t*)state->buffer)[1];
      uint32_t writeoff = ((uint32_t*)state->buffer)[2];

      if(readoff != writeoff) {

	DBG("Xen-Select: found xserver for domain %d had data\n", next_domain->domain);

	int ret = receiver_process_data(&(next_domain->x_receiver));

	if(ret > 0) {
	  if(ret == 1)
	    printf("Destroying xserver for domain %d due to a socket error\n", next_domain->domain);
	  else {
	    printf("Odd return from X server message dispatch! Got %d\n", ret);
	  }

	  int domain_destroyed = destroy_xserver(next_domain, domain_list_head_p, global_state);
	  if(domain_destroyed) {
	    next_domain = next_next_domain;
	    continue;
	  }

	}
	else {

	  if(ret == -1) 
	    work_remaining = 1;
	  
	  // Notify not-full
	  if(xc_evtchn_notify(evtchnhandle, state->evtid) == -1) {
	    printf("Failed to notify not-full for an X extension or 3D client\n");
	  }

	}

      }

    }

    struct client_list_entry* next_client = next_domain->clients;
    
    while(next_client) {
      
      struct client_list_entry* next_next_client = next_client->next;
      
      struct xidc_receiver_state* state = (struct xidc_receiver_state*)next_client->receiver.receive_arg;

      uint32_t send_bufsize = ((uint32_t*)state->return_buffer)[0];
      uint32_t send_readoff = ((uint32_t*)state->return_buffer)[1];
      uint32_t send_writeoff = ((uint32_t*)state->return_buffer)[2];

      uint32_t readoff = ((uint32_t*)state->buffer)[1];
      uint32_t writeoff = ((uint32_t*)state->buffer)[2];
      
      if(next_client->receiver.partial_send && ring_write_space(send_readoff, send_writeoff, send_bufsize)) {

	int sent = next_client->receiver.send(next_client->receiver.send_arg,
					      next_client->receiver.partial_send +
					      next_client->receiver.bytes_sent,
					      next_client->receiver.partial_send_length - 
					      next_client->receiver.bytes_sent);

	if(sent == -1) {
	  printf("Destroying Gallium client due to a send failure\n");
	  int domain_destroyed = destroy_client(next_client, &(next_domain->clients), next_domain, domain_list_head_p, global_state);
	  if(domain_destroyed) {
	    break; // Go to next_domain = next_next_domain
	  }
	}
	else {
	  next_client->receiver.bytes_sent += sent;
	  if(next_client->receiver.bytes_sent == next_client->receiver.partial_send_length) {
	    free(next_client->receiver.partial_send);
	    next_client->receiver.partial_send = 0;
	  }
	}

	xc_evtchn_notify(evtchnhandle, state->return_evtid);
	
      }
      if((!next_client->receiver.partial_send) && readoff != writeoff) {      
	
	DBG("Select: found a client for domain %d\n", next_domain->domain);
	
	int ret = receiver_process_data(&(next_client->receiver));
	
	if(ret > 0) {
	  if(ret == 1)
	    printf("Destroying client due to a transport error\n");
	  else if(ret == 2)
	    printf("Destroying client due to a failed command\n");
	  else {
	    DBG("Odd return from dispatch-message! Got %d\n", ret);
	  }

	  int domain_destroyed = destroy_client(next_client, &(next_domain->clients), next_domain, domain_list_head_p, global_state);
	  if(domain_destroyed) {
	    break; // Go to next_domain = next_next_domain
	  }
	    
	}
	else {

	  if(ret == -1)
	    work_remaining = 1;

	  if(xc_evtchn_notify(evtchnhandle, state->evtid) == -1) {
	    printf("Failed to notify not-full for a 3D client\n");
	  }

	}

      }
      
      next_client = next_next_client;
	
    }

    next_domain = next_next_domain;

  }

  DBGSEL("Done processing Xen events\n");

  return work_remaining;

}

void main_loop(struct xs_handle* xenstorehandle, int xenstorefd, int evtchnhandle, int evtchnfd, int qemulistenfd, struct global_state* global_state) {

  struct domain_list_entry* domain_list_head = 0;
  global_state->domain_list_head = &domain_list_head;
  fd_set read_fds;

  struct timeval timeout;

  struct timeb current_time;
  unsigned int last_frame_time = 0;

  struct timeb start_time;
  unsigned int start_ms;

  ftime(&start_time);

  start_ms = (start_time.time * 1000) + start_time.millitm;
  
  DBG("Initialization complete: xenstore fd is %d, qemu fd is %d\n", xenstorefd, qemulistenfd);

  int xen_work_remaining = 0;

  while(true) {

    timeout.tv_sec = 0;
    timeout.tv_usec = xen_work_remaining ? 0 : 10000;
    int highestfd = 0;

    FD_ZERO(&read_fds);

    FD_SET(xenstorefd, &read_fds);
    highestfd = xenstorefd + 1;

    FD_SET(qemulistenfd, &read_fds);
    if(highestfd <= qemulistenfd)
      highestfd = qemulistenfd + 1;

    FD_SET(evtchnfd, &read_fds);
    if(evtchnfd >= highestfd)
      highestfd = evtchnfd + 1;

    DBGSEL("Select: added fd %d (Xenstore listen)\n", xenstorefd);
    DBGSEL("Select: added fd %d (Qemu-dm listen)\n", qemulistenfd);
    DBGSEL("Select: added fd %d (Xen-event listen)\n", evtchnfd);

    struct domain_list_entry* next_domain = domain_list_head;

    while(next_domain) {

      if(next_domain->qemu_receiver.receive_arg) {
	struct tcp_receiver_state* state = (struct tcp_receiver_state*)next_domain->qemu_receiver.receive_arg;
	FD_SET(state->fd, &read_fds);
	DBGSEL("Added Dom %d / fd %d (Qemu-dm)\n", next_domain->domain, state->fd);
	if(state->fd > highestfd)
	  highestfd = state->fd;
      }
      
      next_domain = next_domain->next;
      
    }

    DBGSEL("Select: polling...\n");

    int fds_to_do = select(highestfd + 1, &read_fds, NULL, NULL, &timeout);

    DBGSEL("Select: returned %d fds to handle\n", fds_to_do);

    if(fds_to_do == -1) {
      printf("Select failed\n");
      exit(1);
    }

    if(FD_ISSET(xenstorefd, &read_fds)) {
      DBGSEL("Select: found a Xenstore watch had fired\n");
      handle_xenstore_event(xenstorehandle, evtchnhandle, &domain_list_head, global_state);

      fds_to_do--;
    }

    if(xen_work_remaining || FD_ISSET(evtchnfd, &read_fds)) {
      DBGSEL("Select: found an event channel had fired\n");
      xen_work_remaining = handle_xen_event(&domain_list_head, evtchnhandle, evtchnfd, global_state);

      if(xen_work_remaining == -1) {
	printf("Fatal error processing XIDC clients\n");
	exit(1);
      }

      if(FD_ISSET(evtchnfd, &read_fds)) 
	fds_to_do--;
    }
    
    if(FD_ISSET(qemulistenfd, &read_fds)) {
      DBGSEL("Select: found qemulistenfd active; handling...\n");
      handle_new_qemu_connection(&domain_list_head, qemulistenfd, global_state);
      fds_to_do--;
    }

    next_domain = domain_list_head;

    while(fds_to_do && next_domain) {
    
      struct domain_list_entry* next_next_domain = next_domain->next;

      struct tcp_receiver_state* state = (struct tcp_receiver_state*)next_domain->qemu_receiver.receive_arg;

      if(state && FD_ISSET(state->fd, &read_fds)) {

	DBGSEL("Select: found qemu for domain %d had data\n", next_domain->domain);

	int ret = receiver_process_data(&(next_domain->qemu_receiver));

	fds_to_do--;

	if(ret > 0) {
	  if(ret == 1)
	    printf("Destroying qemu of domain %d due to a socket error\n", next_domain->domain);
	  else {
	    DBG("Odd return from qemu message dispatch! Got %d\n", ret);
	  }

	  int domain_destroyed = destroy_qemu(next_domain, &domain_list_head, global_state);
	  if(domain_destroyed) {
	    next_domain = next_next_domain;
	    continue;
	  }
	  
	}

      }
      
      next_domain = next_next_domain;

    }

    DBGSEL("Done processing incoming data: processing input events\n");
    
    int switch_domains = handle_x_events(global_state);

    if(switch_domains) {
      if(!notify_controller(global_state)) {
	printf("No domain controller connected; could not notify\n");
      }
      /*      if(!switch_to_domain(global_state, global_state->trusted_domain)) {
	printf("Couldn't switch to domain %d\n", global_state->trusted_domain);
	}*/
      // The trusted domain is now drawn whenever global_state->control_dom_state == CONTROL_DOM_VISIBLE
    }
    
    DBGSEL("Done processing input events: drawing master context\n");

    ftime(&current_time);

    unsigned int difference = ((current_time.time * 1000) + current_time.millitm) - last_frame_time;

    if(difference > 10) {
      draw_master_context(global_state);
      last_frame_time = (current_time.time * 1000) + current_time.millitm;
    }

  }

}


static void
make_x_window(Display *x_dpy, EGLDisplay egl_dpy,
	      const char *name,
	      int x, int y, int width, int height,
	      Window *winRet,
	      EGLContext *ctxRet,
	      EGLSurface *surfRet,
	      EGLConfig *configRet)
{
  static const EGLint attribs[] = {
    EGL_RED_SIZE, 1,
    EGL_GREEN_SIZE, 1,
    EGL_BLUE_SIZE, 1,
    EGL_DEPTH_SIZE, 1,
    EGL_NONE
  };

  int scrnum;
  XSetWindowAttributes attr;
  unsigned long mask;
  Window root;
  Window win;
  XVisualInfo *visInfo, visTemplate;
  int num_visuals;
  EGLContext ctx;
  EGLConfig config;
  EGLint num_configs;
  EGLint vid;

  scrnum = DefaultScreen( x_dpy );
  root = RootWindow( x_dpy, scrnum );

  if (!eglChooseConfig( egl_dpy, attribs, &config, 1, &num_configs)) {
    printf("Error: couldn't get an EGL visual config\n");
    exit(1);
  }

  assert(config);
  assert(num_configs > 0);

  if (!eglGetConfigAttrib(egl_dpy, config, EGL_NATIVE_VISUAL_ID, &vid)) {
    printf("Error: eglGetConfigAttrib() failed\n");
    exit(1);
  }

  /* The X window visual must match the EGL config */
  visTemplate.visualid = vid;
  visInfo = XGetVisualInfo(x_dpy, VisualIDMask, &visTemplate, &num_visuals);
  if (!visInfo) {
    printf("Error: couldn't get X visual\n");
    exit(1);
  }
  
  /* window attributes */
  attr.background_pixel = 0;
  attr.border_pixel = 0;
  attr.colormap = XCreateColormap( x_dpy, root, visInfo->visual, AllocNone);
  attr.event_mask = StructureNotifyMask | ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
  mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

  win = XCreateWindow( x_dpy, root, 0, 0, width, height,
		       0, visInfo->depth, InputOutput,
		       visInfo->visual, mask, &attr );

  /* set hints and properties */
  {
    XSizeHints sizehints;
    sizehints.x = x;
    sizehints.y = y;
    sizehints.width  = width;
    sizehints.height = height;
    sizehints.flags = USSize | USPosition;
    XSetNormalHints(x_dpy, win, &sizehints);
    XSetStandardProperties(x_dpy, win, name, name,
			   None, (char **)NULL, 0, &sizehints);
  }

  eglBindAPI(EGL_OPENGL_API);

  ctx = eglCreateContext(egl_dpy, config, EGL_NO_CONTEXT, NULL );
  if (!ctx) {
    printf("Error: eglCreateContext failed\n");
    exit(1);
  }

  *surfRet = eglCreateWindowSurface(egl_dpy, config, win, NULL);

  if (!*surfRet) {
    printf("Error: eglCreateWindowSurface failed\n");
    exit(1);
  }

  XFree(visInfo);

  *winRet = win;
  *ctxRet = ctx;
  *configRet = config;

}

int main(int argc, char** argv) {

  // Part 1: Network setup

  struct xs_handle* xsh = xs_domain_open();

  if(!xsh) {
    printf("Failed to open Xenstore\n");
    return 1;
  }

  xs_rm(xsh, XBT_NULL, "/local/domain/0/backend/xen3d");

  DBG("Deleted my node\n");

  struct xs_permissions perms;

  bool ret = xs_mkdir(xsh, XBT_NULL, WATCH_NODE);
  if(!ret) {
    printf("Couldn't mkdir\n");
    return 1;
  }

  DBG("Created %s\n", WATCH_NODE);

  // Make it world-writable                                                     
  perms.id = 0;
  perms.perms = XS_PERM_WRITE;
  ret = xs_set_permissions(xsh, XBT_NULL, WATCH_NODE, &perms, 1);

  DBG("Set that as world-writable\n");

  int ret2 = xs_watch(xsh, WATCH_NODE, "watch");
  if(ret2 == -1) {
    printf("Failed to watch the watchnode\n");
    return 1;
  }

  DBG("Set my watch\n");

  int xenstorefd = xs_fileno(xsh);

  setnonblocking(xenstorefd);

  int xceh = xc_evtchn_open();

  if(xceh == -1) {
    printf("Failed to open evtchn\n");
    return 1;
  }
  else {
    printf("Opened evtchn: got handle %d\n");
  }

  int evtchnfd = xc_evtchn_fd(xceh);

  setnonblocking(evtchnfd);

  int qemulistenfd = socket(PF_UNIX, SOCK_STREAM, 0);
  if(qemulistenfd == -1) {
    printf("socket() failed (Unix domain)\n");
    exit(1);
  }
    
  struct sockaddr_un un_bind_address;
  memset(&un_bind_address, 0, sizeof(struct sockaddr_un));
  un_bind_address.sun_family = AF_UNIX;

  unlink("/var/run/xen3d-socket");
  strcpy(un_bind_address.sun_path, "/var/run/xen3d-socket");
  
  ret = bind(qemulistenfd, (struct sockaddr*)&un_bind_address, sizeof(un_bind_address.sun_family) + strlen(un_bind_address.sun_path));
  if(ret == -1) {
    printf("bind() failed (Unix domain)\n");
    exit(1);
  }
    
  ret = listen(qemulistenfd, 1);
  if(ret == -1) {
    printf("listen() failed (Unix domain)\n");
    exit(1);
  }

  setnonblocking(qemulistenfd);

  struct global_state global_state;

  // Part 2: graphics context setup

  const int winWidth = 800, winHeight = 600;
  Display *x_dpy;
  Window win;
  EGLSurface egl_surf;
  EGLContext egl_ctx;
  EGLDisplay egl_dpy;
  EGLConfig egl_conf;
  char *dpyName = NULL;
  EGLint egl_major, egl_minor;
  int i;

  x_dpy = XOpenDisplay(dpyName);
  if (!x_dpy) {
    printf("Error: couldn't open display %s\n",
	   dpyName ? dpyName : getenv("DISPLAY"));
    return -1;
  }

  egl_dpy = eglGetDisplay(x_dpy);
  if (!egl_dpy) {
    printf("Error: eglGetDisplay() failed\n");
    return -1;
  }

  if (!eglInitialize(egl_dpy, &egl_major, &egl_minor)) {
    printf("Error: eglInitialize() failed\n");
    return -1;
  }
  
  make_x_window(x_dpy, egl_dpy,
		"Xen3D Compositor", 0, 0, winWidth, winHeight,
		&win, &egl_ctx, &egl_surf, &egl_conf);

  XMapWindow(x_dpy, win);

  if(!eglMakeCurrent(egl_dpy, egl_surf, egl_surf, egl_ctx)) {
    printf("Couldn't make the master context current for the first time\n");
    exit(1);
  }

  struct pipe_context* current_context = get_current_pipe_context();

  // Now create a hermes instance, and a format for copying to the window.

  if(!Hermes_Init()) {
    printf("Failed to initialise Hermes pixel-conversion library\n");
    return 1;
  }

  if(!(global_state.hermes_handle = Hermes_ConverterInstance(0))) {
    printf("Failed to get Hermes converter\n");
    return 1;
  }

  void* opaqueContext = get_current_st_context();
  if(!opaqueContext) {
    printf("Failed to get current context from raw state tracker\n");
    return 1;
  }

  struct pipe_surface* backbuffer = get_context_renderbuffer_surface(opaqueContext, 1);

  if(!backbuffer) {
    printf("Trying to determine backbuffer's format, but there is no backbuffer\n");
    return 1;
  }

  int win_depth = 0;
  int win_pixel_size = 0;

  int32 rmask, gmask, bmask, amask = 0;

  switch(backbuffer->format) {

  case PIPE_FORMAT_A8R8G8B8_UNORM:
    amask = 0xFF000000;
    rmask = 0x00FF0000;
    gmask = 0x0000FF00;
    bmask = 0x000000FF;
    win_depth = 32;
    win_pixel_size = 32;
    break;
  case PIPE_FORMAT_R8G8B8_UNORM:
    rmask = 0x00FF0000;
    gmask = 0x0000FF00;
    bmask = 0x000000FF;
    win_depth = 24;
    win_pixel_size = 24;
    break;
  case PIPE_FORMAT_X8R8G8B8_UNORM:
    rmask = 0x00FF0000;
    gmask = 0x0000FF00;
    bmask = 0x000000FF;
    win_depth = 24;
    win_pixel_size = 32;
    break;
  case PIPE_FORMAT_R5G6B5_UNORM:
    rmask = 0x0000F800;
    gmask = 0x000007E0;
    bmask = 0x0000001F;
    win_depth = 16;
    win_pixel_size = 16;
    break;
  default:
    printf("window backbuffer had an unsupported format (allowed: depth 16, 24, 32, packed or unpacked, RGB ordered)\n");
    return 1;
  }

  global_state.hermes_dest_format = Hermes_FormatNew(win_pixel_size, rmask, gmask, bmask, amask, 0);
  printf("Created a Hermes format for depth %d, pixel-size %d\n", win_depth, win_pixel_size);

  global_state.screen = current_context->screen;

  global_state.x_display = x_dpy;

  global_state.master_egl_context = egl_ctx;
  global_state.egl_display = egl_dpy;
  global_state.window_egl_surface = egl_surf;
  global_state.egl_config = egl_conf;

  global_state.window_width = winWidth;
  global_state.window_height = winHeight;
  
  global_state.window_depth = win_depth;
  global_state.window_pixel_size = win_pixel_size;

  global_state.window_pipe_format = backbuffer->format;
  
  global_state.screen_handles = 1;
  
  global_state.current_domain = 0;

  global_state.trusted_domain = 0;
  global_state.trusted_domain_descriptor = 0;

  global_state.control_dom_state = CONTROL_DOM_STATE_INVISIBLE;
  global_state.control_dom_off_x = 0;
  global_state.control_dom_off_y = 0;
  global_state.control_dom_n_clip_rects = 0;
  global_state.control_dom_clip_rects = 0;
  
  memset(&global_state.domain_control_receiver, 0, sizeof(struct receiver));

  init_master_context(&global_state);

  for(int i = 0; i < argc; i++) {

    if(!strncmp(argv[i], "--trusted-domain", 16)) {
      if((i + 1) >= argc) {
	printf("trusted domain argument needs an option\n");
      }
      else {
	sscanf(argv[i+1], "%d", &global_state.trusted_domain);
	printf("Making domain %d trusted\n", global_state.trusted_domain);
      }
    }

  }

  main_loop(xsh, xenstorefd, xceh, evtchnfd, qemulistenfd, &global_state);

  eglDestroyContext(egl_dpy, egl_ctx);
  eglDestroySurface(egl_dpy, egl_surf);
  eglTerminate(egl_dpy);

  XDestroyWindow(x_dpy, win);
  XCloseDisplay(x_dpy);
    
}




    
  

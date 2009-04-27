
#include <sys/un.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#include <xenctrl.h>
#include <xs.h>
#include <xen/sys/gntmem.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "picker.h"

struct xidc_state {

  int fd;
  uint32_t rx_grants[4];
  uint32_t tx_grants[4];
  void* rx_buffer;
  void* tx_buffer;

};

// forward declarations
void send_message(struct xidc_state*, struct xen3d_control_message*);

void create_xidc_channel(struct xidc_state* state) {

  struct sockaddr_un sun;

  sun.sun_family = AF_UNIX;
  strcpy(sun.sun_path, "/var/run/xen3dd-socket");

  int socketfd = socket(PF_UNIX, SOCK_STREAM, 0);

  if(socketfd == -1) {
    printf("socket() failed\n");
    exit(1);
  }

  if(connect(socketfd, (struct sockaddr*)&sun, sizeof(sun.sun_family) + strlen(sun.sun_path)) < 0) {
    printf("Couldn't connect to /var/run/xen3dd-socket\n");
    exit(1);
  }

  // Now create two initally-empty ring buffers, one for RX and one for TX.

  struct {
    uint32_t* grants;
    void** map;
  } togrant[2] = {
    { .grants = state->rx_grants, .map = &(state->rx_buffer) },
    { .grants = state->tx_grants, .map = &(state->tx_buffer) }
  };

  for(int i = 0; i < 2; i++) {
    int fd = open("/dev/gntmem", O_RDWR);
    int ret;

    if(fd == -1) {
      printf("Failed to open /dev/gntmem\n");
      exit(1);
    }

    ret = ioctl(fd, IOCTL_GNTMEM_SET_DOMAIN, 0);

    if(ret == -1) {
      printf("Failed to set gntmem's target domain\n");
      exit(1);
    }

    ret = ioctl(fd, IOCTL_GNTMEM_SET_SIZE, 4);

    if(ret == -1) {
      printf("Failed to create a shareable section of 4 pages\n");
      exit(1);
    }

    ret = ioctl(fd, IOCTL_GNTMEM_GET_GRANTS, togrant[i].grants);

    if(ret == -1) {
      printf("Failed to get grants for shared section\n");
      exit(1);
    }

    *(togrant[i].map) = (void*)-1;
    *(togrant[i].map) = mmap(0, 4096 * 4, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if(*(togrant[i].map) == (void*)-1) {
      printf("Couldn't mmap our shared section\n");
      exit(1);
    }

    close(fd);
    // Will remain in existence until an munmap                                  

    // Now init the ring-buffer before we announce to the daemon                 
    uint32_t* initdata = (uint32_t*)*(togrant[i].map);
    uint32_t buffersize = (4096 * 4) - (sizeof(uint32_t) * 3);
    initdata[0] = buffersize;
    // total buffer size                                                        \
                                                                                  
    initdata[1] = 0;
    initdata[2] = 0;
    // Next byte to read and next-to-write respectively.                         
    // Equality signals the buffer is empty, write = read - 1 signals it is full
  }

  // Okay, both ring buffers exist. Now send the grants to the daemon.           
  struct {
    uint32_t rx_grants[4];
    uint32_t tx_grants[4];
    char client_type;
  } tosend;

  for(int i = 0; i < 4; i++) {
    tosend.rx_grants[i] = state->rx_grants[i];
    tosend.tx_grants[i] = state->tx_grants[i];
  }

  tosend.client_type = 2;
  // This is a domain controller

  int bytessent = 0;

  while(bytessent < sizeof(tosend)) {

    int sent = send(socketfd, ((char*)&tosend) + bytessent, sizeof(tosend) - bytessent, 0);

    if(sent <= 0) {
      printf("Failed to send init message to xen3dd: return %d with errno %d\n", sent, errno);
      exit(1);
    }
    else {
      bytessent += sent;
    }

  }

  state->fd = socketfd;

}

int wait_for_char(int fd, char c) {

  char received = c + 1;

  while(received != c) {
    int ret = recv(fd, &received, 1, 0);
    if(ret <= 0) {
      printf("Error waiting for a %c (return %d, errno %d)\n", c, ret, errno);
      return -1;
    }
  }

  return 0;

}

int wait_for_tx_not_full(int fd) {

  return wait_for_char(fd, 'T');

}

int wait_for_rx_not_empty(int fd) {

  return wait_for_char(fd, 'R');

}

uint32_t read_avail(uint32_t* readoff, uint32_t* writeoff, uint32_t buffersize) {

  uint32_t limit = *writeoff;
  uint32_t base = *readoff;

  if(limit > base)
    return limit - base;
  else if(limit == base)
    return 0;
  else
    return buffersize - (base - limit);

}

int read_would_wrap(uint32_t* readoff, uint32_t buffersize, uint32_t bytes) {

  return (((*readoff) + bytes) > buffersize);

}

int recvsome(uint32_t* readoff, uint32_t* writeoff, uint32_t bufsize, char* readbuf, void* outbuf, int length) {

  int available = (int)read_avail(readoff, writeoff, bufsize);

  if(read_would_wrap(readoff, bufsize, length)) {
    int bytestoend = (bufsize - *readoff);
    memcpy(outbuf, readbuf + (*readoff), bytestoend);
    memcpy(((char*)outbuf) + bytestoend, readbuf, length - bytestoend);
  }
  else {
    memcpy(outbuf, readbuf + (*readoff), length);
  }

  return length;

}


void wait_for_prompt(struct xidc_state* state) {

  uint32_t bufsize = ((uint32_t*)state->rx_buffer)[0];
  uint32_t* readoff = &(((uint32_t*)state->rx_buffer)[1]);
  uint32_t* writeoff = &(((uint32_t*)state->rx_buffer)[2]);
  char* readbuf = (char*)&(((uint32_t*)state->rx_buffer)[3]);

  int avail;

  while((avail = read_avail(readoff, writeoff, bufsize)) < sizeof(struct xen3d_control_message_show)) {
    wait_for_rx_not_empty(state->fd);
  }

  struct xen3d_control_message_show message;

  recvsome(readoff, writeoff, bufsize, readbuf, &message, sizeof(struct xen3d_control_message_show));

  uint32_t newreadoff = *readoff + sizeof(struct xen3d_control_message_show);
  if(newreadoff >= bufsize)
    newreadoff -= bufsize;

  *readoff = newreadoff;

  send(state->fd, "R", 1, 0);

  if(message.base.opcode != XEN3D_CONTROL_MESSAGE_SHOW || message.base.length != sizeof(struct xen3d_control_message_show)) {
    printf("Received a message other than show!\n");
  }

  printf("Notified; told that the existing domain is %d, which has size (%d, %d)\n", message.current_domain, message.backdrop_width, message.backdrop_height);

  return;

}

int sendsome(uint32_t* readoff, uint32_t* writeoff, uint32_t bufsize, char* writebuf, char* data, int length) {

  uint32_t base = *writeoff;
  uint32_t limit = *readoff;
  uint32_t space;
  int writewrapsbuffer;

  if(limit > base) {
    space = (limit - base) - 1;
  }
  else {
    space = base - limit;
    space = (bufsize - space) - 1;
    // Care! > rather than >= because if it == buffersize, the write does         
    // not wrap, but the pointer does                                             
  }

  int towrite = space >= length ? length : space;

  if((base + towrite) > bufsize)
    writewrapsbuffer = 1;
  else
    writewrapsbuffer = 0;

  if(towrite == 0) {
    // Looks like the buffer is full: let's wait                                  
    return 0;
  }

  // Okay, we have (towrite) bytes to write: let's do that.                       
  if(!writewrapsbuffer) {
    memcpy(writebuf + base, data, towrite);
  }
  else {
    int bytestoend = bufsize - base;
    memcpy(writebuf + base, data, bytestoend);
    memcpy(writebuf, data + bytestoend, towrite - bytestoend);
  }

  return towrite;

}


void send_message(struct xidc_state* state, struct xen3d_control_message* message) {

  uint32_t bufsize = ((uint32_t*)state->tx_buffer)[0];
  uint32_t* readoff = &(((uint32_t*)state->tx_buffer)[1]);
  uint32_t* writeoff = &(((uint32_t*)state->tx_buffer)[2]);
  char* writebuf = (char*)&(((uint32_t*)state->tx_buffer)[3]);

  int bytesSent = 0;

  while(bytesSent < message->length) {

    int sent = sendsome(readoff, writeoff, bufsize, writebuf, (((char*)message) + bytesSent), message->length - bytesSent);

    if(sent == 0) {
      if(wait_for_tx_not_full(state->fd) == -1) {
        printf("Socket error waiting for tx-not-full\n");
        exit(1);
      }
    }

    uint32_t newwriteoff = *writeoff + sent;
    if(newwriteoff >= bufsize)
      newwriteoff -= bufsize;

    *writeoff = newwriteoff;

    bytesSent += sent;

    // Notify the remote that we have sent something                              
    send(state->fd, "T", 1, 0);

  }

}

int ask_user_for_domain() {

  int answer = -1;

  while(answer == -1) {

    printf("Domain to switch to? ");
    scanf("%d", &answer);
    printf("Ta\n");

  }

  return answer;

}

void notify_switch(struct xidc_state* state, int domain) {

  struct xen3d_control_message_choose_domain message;

  message.base.opcode = XEN3D_CONTROL_MESSAGE_CHOOSE_DOMAIN;
  message.base.length = sizeof(struct xen3d_control_message_choose_domain);
  message.domain = (uint32_t)domain;

  send_message(state, (struct xen3d_control_message*)&message);

}  

void declare_full_visibility(struct xidc_state* state) {

  int length = sizeof(struct xen3d_control_message_set_clip) + (2 * sizeof(struct xen3d_clip_rect));

  char* message = malloc(length);

  if(!message) {
    printf("Malloc failed!\n");
    exit(1);
  }

  struct xen3d_control_message_set_clip* header =
    (struct xen3d_control_message_set_clip*)message;

  struct xen3d_clip_rect* rect =
    (struct xen3d_clip_rect*)(message + sizeof(struct xen3d_control_message_set_clip));

  header->base.opcode = XEN3D_CONTROL_MESSAGE_SET_CLIP;
  header->base.length = length;
  header->offset_x = 10;
  header->offset_y = 10;
  header->nrects = 2;

  rect[0].x = 0;
  rect[0].y = 0;
  rect[0].w = 500;
  rect[0].h = 500;

  rect[1].x = 500;
  rect[1].y = 500;
  rect[1].w = 500;
  rect[1].h = 500;

  /*  printf("Sending bytes: ");
  for(int i = 0; i < length; i++) {
    printf("%d ", (int)message[i]);
  }
  printf("\n");
  */
  send_message(state, (struct xen3d_control_message*)message);

  free(message);

}

void notify_ready(struct xidc_state* state) {

  struct xen3d_control_message message;
					       
  message.opcode = XEN3D_CONTROL_MESSAGE_READY;
  message.length = sizeof(struct xen3d_control_message);

  send_message(state, &message);

}

int main(int argc, char** argv) {

  struct xidc_state connstate;

  create_xidc_channel(&connstate);

  while(true) {

    wait_for_prompt(&connstate);

    declare_full_visibility(&connstate);
    printf("Declared full visibility\n");
    notify_ready(&connstate);
    printf("Notified ready to display\n");
    
    int domain = ask_user_for_domain();
    
    notify_switch(&connstate, domain);

  }

}

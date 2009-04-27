
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>

#include <sys/types.h>
#include <sys/socket.h>

#include "pipe/p_compiler.h"

#include "winsys_bindings.h"
#include "tr_screen.h"
#include "remote_messages.h"
#include "remote_comms.h"
#include "remote_debug.h"

int last_alloc_is_in_place = 0;
// XXX this relies on no two calls to allocate_message_memory without an
// enqueue_message in between. If this changes, need to start properly tracking
// whether each alloc is in-place or not.

int last_read_is_in_place = 0;

extern int analyse_waits;

void check_for_asynchronous_failures(void);
void wait_for_synchronous_message(void);

int drain_events(int fd) {

  int flags = fcntl(fd, F_GETFL);

  int nbflags = flags | O_NONBLOCK;

  fcntl(fd, F_SETFL, nbflags);

  char waste[1024];

  int ret;
  while((ret = recv(fd, waste, 1024, 0)) > 0) {

  }

  fcntl(fd, F_SETFL, flags);

  if(ret == -1 && errno == EAGAIN)
    return 0;
  else {
    printf("Failed draining events: return %d, errno %d\n", ret, errno);
    return -1;
  }

}

int wait_for_char(int fd, char c) {

  char received = c + 1;

  DBG("Waiting for a %c\n", c);

  while(received != c) {
    int ret = recv(fd, &received, 1, 0);
    DBG("Got a char: %c\n", received);
    if(ret <= 0) {
      printf("Error waiting for a %c (return %d, errno %d)\n", c, ret, errno);
      return -1;
    }
  }

  DBG("That's what I was looking for; returning success\n");

  return 0;

}

int wait_for_tx_not_full() {

  DBG("Waiting for an event on the transmit ring\n");
  return wait_for_char(singleton_screen->socketfd, 'T');

}

int wait_for_rx_not_empty() {

  DBG("Waiting for an event on the read-ring\n");
  return wait_for_char(singleton_screen->socketfd, 'R');

}

void* allocate_message_memory(size_t bytes) {

  uint32_t bufsize = ((uint32_t*)singleton_screen->tx_buffer)[0];
  uint32_t readoff = ((uint32_t*)singleton_screen->tx_buffer)[1];
  uint32_t writeoff = ((uint32_t*)singleton_screen->tx_buffer)[2];
  char* writebuf = (char*)&(((uint32_t*)singleton_screen->tx_buffer)[3]);

  int contiguous_available_space;

  if(readoff > writeoff)
    contiguous_available_space = (readoff - writeoff) - 1;
  else {
    contiguous_available_space = (bufsize - writeoff);
    if(readoff == 0)
      contiguous_available_space--;
    /* We can write up to the end of the buffer, except if the read pointer is at 0;
       if it is, writing to the end would set write == read, and therefore empty the buffer,
       discarding a full ring. */
  }

  struct message_header* new_message = 0;

  if(contiguous_available_space >= bytes) {
    new_message = (struct message_header*)(writebuf + writeoff);
    last_alloc_is_in_place = 1;
  }
  else {
    new_message = (struct message_header*)malloc(bytes);
    if(!new_message) {
      printf("Malloc failed allocating a message (wanted %d bytes)\n", (int)bytes);
      exit(1);
    }
    last_alloc_is_in_place = 0;
  }

   new_message->length = bytes;

   return new_message;

}

void debug_print_bytes(char* bytes, int n) {

  for(int i = 0; i < n; i++) {
    printf("%x ", (unsigned int)(bytes[i]));
    if(i && (!(i % 16)))
      printf("\n");
  }

  printf("\n");

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

  DBG("WRITING: Will write %d bytes (current r/w %u/%u)\n", towrite, limit, base);
  //DBG("Those bytes of the user's buffer:\n");
  //debug_print_bytes(data, towrite);

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
    DBG("Wrote %d bytes (contiguous)\n", towrite);
    memcpy(writebuf + base, data, towrite);
  }
  else {
    int bytestoend = bufsize - base;
    memcpy(writebuf + base, data, bytestoend);
    DBG("Wrote %d bytes at buffer end\n", bytestoend);
    memcpy(writebuf, data + bytestoend, towrite - bytestoend);
    DBG("Wrote %d bytes at buffer start\n", towrite - bytestoend);
  }

  return towrite;

}

void enqueue_message(void* message) {

  struct message_header* header = (struct message_header*)message;
  uint32_t bufsize = ((uint32_t*)singleton_screen->tx_buffer)[0];
  uint32_t* readoff = &(((uint32_t*)singleton_screen->tx_buffer)[1]);
  uint32_t* writeoff = &(((uint32_t*)singleton_screen->tx_buffer)[2]);
  char* writebuf = (char*)&(((uint32_t*)singleton_screen->tx_buffer)[3]);

  DBG("Sending %s (length %u)\n", optotext(header->opcode), header->length);

  check_for_asynchronous_failures();

  if(last_alloc_is_in_place) {

    uint32_t newwriteoff = *writeoff + header->length;
    if(newwriteoff >= bufsize)
      newwriteoff -= bufsize;
    
    *writeoff = newwriteoff;

    send(singleton_screen->socketfd, "T", 1, 0);
    return;
  }
    
  int bytesSent = 0;

  while(bytesSent < header->length) {

    if(drain_events(singleton_screen->socketfd) == -1) {
      printf("Socket error draining events\n");
      exit(1);
    }

#ifdef DEBUG_RING_WRITE
    uint32_t base = *readoff;
    uint32_t limit = *writeoff;

    int bytes_available_for_readers;
    if(limit > base)
      bytes_available_for_readers = limit - base;
    else
      bytes_available_for_readers = bufsize - (base - limit);

    int read_would_wrap = 0;
    if((base + bytes_available_for_readers) > bufsize)
      read_would_wrap = 1;

    printf("Writing with readoff %u, writeoff %u: dumping unread bytes\n", base, limit);

    if(!read_would_wrap) {
      debug_print_bytes(writebuf + base, bytes_available_for_readers);
    }
    else {
      int bytes_to_end = bufsize - base;
      debug_print_bytes(writebuf + base, bytes_to_end);
      printf("---buffer end---\n");
      debug_print_bytes(writebuf, bytes_available_for_readers - bytes_to_end);
    }

#endif // DEBUG_RING_WRITE
     
    int sent = sendsome(readoff, writeoff, bufsize, writebuf, ((char*)header + bytesSent), header->length - bytesSent);

    if(sent == 0) {
      if(analyse_waits)
	printf("Wait: transmit buffer full... ");
      if(wait_for_tx_not_full() == -1) {
	printf("Socket error waiting for tx-not-full\n");
	exit(1);
      }
      if(analyse_waits)
	printf("Woke to find read %u, write %u\n", *readoff, *writeoff);
    }

    uint32_t newwriteoff = *writeoff + sent;
    if(newwriteoff >= bufsize)
      newwriteoff -= bufsize;

    *writeoff = newwriteoff;

    bytesSent += sent;

    // Notify the remote that we have sent something
    send(singleton_screen->socketfd, "T", 1, 0);

  }
   
  free(header);

}

uint32_t read_avail(uint32_t* readoff, uint32_t* writeoff, uint32_t buffersize) {

  uint32_t limit = *writeoff;
  uint32_t base = *readoff;

  DBG("read-avail: found limit = %u, base = %u\n", limit, base);

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

  if(!available)
    return 0;

  int tocopy = available < length ? available : length;

  if(read_would_wrap(readoff, bufsize, tocopy)) {
    int bytestoend = (bufsize - *readoff);
    memcpy(outbuf, readbuf + (*readoff), bytestoend);
    memcpy(((char*)outbuf) + bytestoend, readbuf, tocopy - bytestoend);
  }
  else {
    memcpy(outbuf, readbuf + (*readoff), tocopy);
  }

  return tocopy;

}

static void discard_ring_bytes(uint32_t* readoff, uint32_t bufsize, int bytes_to_discard) {

  uint32_t newreadoff = *readoff + bytes_to_discard;
  if(newreadoff >= bufsize)
    newreadoff -= bufsize;
  
  *readoff = newreadoff;
  send(singleton_screen->socketfd, "R", 1, 0);
  
}


static void partial_recv(uint32_t* readoff, uint32_t* writeoff, uint32_t bufsize, char* readbuf, struct message_header* message, int* current_bytes, int target_bytes) {

  while(*current_bytes < target_bytes) {

    if(drain_events(singleton_screen->socketfd) == -1) {
      printf("Socket error draining events\n");
      exit(1);
    }

    int this_recv = recvsome(readoff, writeoff, bufsize, readbuf, 
			 (void*)((char*)message + *current_bytes), 
			     target_bytes - *current_bytes);

    if(this_recv == 0) {
      if(analyse_waits)
	printf("Wait: receive buffer empty\n");
      if(wait_for_rx_not_empty() == -1) {
	printf("Socket error waiting for rx-not-empty\n");
	exit(1);
      }
    }

    discard_ring_bytes(readoff, bufsize, this_recv);
    
    *current_bytes += this_recv;

  }

}

struct message_header* get_message() {

  uint32_t bufsize = ((uint32_t*)singleton_screen->rx_buffer)[0];
  uint32_t* readoff = &(((uint32_t*)singleton_screen->rx_buffer)[1]);
  uint32_t* writeoff = &(((uint32_t*)singleton_screen->rx_buffer)[2]);
  char* readbuf = (char*)&(((uint32_t*)singleton_screen->rx_buffer)[3]);

  int size;

  DBG("Entering get_message: readoff = %u, writeoff = %u\n", *readoff, *writeoff);

  int avail;

  wait_for_synchronous_message();

  if(!read_would_wrap(readoff, bufsize, sizeof(struct message_header))) {

    DBG("Looking for a message header's worth of data\n");

    while((avail = read_avail(readoff, writeoff, bufsize)) < sizeof(struct message_header))  { 
      DBG("Waiting for more to be written (currently have %d, wanting %d\n)\n", avail, sizeof(struct message_header)); 
      wait_for_rx_not_empty(); 
    }

    size = ((struct message_header*)(readbuf + *readoff))->length;
    DBG("Found message has length %d\n", size);

    if(read_would_wrap(readoff, bufsize, size)) {
      DBG("Read would wrap; not reading in place\n");
      last_read_is_in_place = 0;
    }
    else {
      DBG("Read would not wrap; reading in place\n");
      last_read_is_in_place = 1;
    }

  }
  else {
    DBG("Reading only a message_header would wrap; not reading in place\n");
    last_read_is_in_place = 0;
  }
  
  if(last_read_is_in_place) {

    DBG("Reading in place\n");

    // Pass out a pointer to the ring buffer in-place, once it is full enough
    while((avail = read_avail(readoff, writeoff, bufsize)) < size)  {
      DBG("Waiting for more to be written (got %d, wanting %d)\n", avail, size);
      wait_for_rx_not_empty();
    }

    DBG("Got enough; passing out in-place pointer\n");
    return (struct message_header*)(readbuf + *readoff);

  }
  else {

    DBG("Not reading in place\n");

    // Malloc a temporary and progressively copy into it
    struct message_header* new_message = (struct message_header*)malloc(sizeof(struct message_header));

    int bytes_received = 0;

    partial_recv(readoff, writeoff, bufsize, readbuf, new_message, &bytes_received, sizeof(struct message_header));

    if(new_message->length < sizeof(struct message_header)) {
      printf("Unreasonably short message encountered whilst receiving (length %d)\n", new_message->length);
      exit(1);
    }

    new_message = (struct message_header*)realloc(new_message, new_message->length);

    partial_recv(readoff, writeoff, bufsize, readbuf, new_message, &bytes_received, new_message->length);
  
    DBG("Received %s (length %u)\n", optotext(new_message->opcode), new_message->length);

    return new_message;
  }

}

void free_message(void* message) {


  if(last_read_is_in_place) {
    // Move the read pointer forwards
    uint32_t bufsize = ((uint32_t*)singleton_screen->rx_buffer)[0];
    uint32_t* readoff = &(((uint32_t*)singleton_screen->rx_buffer)[1]);

    int size = ((struct message_header*)message)->length;

    uint32_t newreadoff = *readoff + size;
    if(newreadoff >= bufsize)
      newreadoff -= bufsize;
    *readoff = newreadoff;

    send(singleton_screen->socketfd, "R", 1, 0);    
    
  }
  else {

    free(message);

  }

}

int message_is_asynchronous(uint32_t opcode) {

  if(opcode == REMREP_TEXTURE_CREATE ||
     opcode == REMREP_TEXTURE_BLANKET ||
     opcode == REMREP_GET_TEX_SURFACE ||
     opcode == REMREP_BUFFER_SET_DATA)
    return 1;
  else
    return 0;

}

void check_async_result(char* message, uint32_t opcode) {

  switch(opcode) {
  case REMREP_TEXTURE_CREATE: {
    struct remrep_texture_create* rep = (struct remrep_texture_create*)message;
    if(rep->success) {
      DBG("Ack: texture_create\n");
    }
    else {
      printf("!!! TEXTURE_CREATE FAILED. This may cause subsequent failures and incorrect rendering\n");
    }
    break;
  }
  case REMREP_TEXTURE_BLANKET: {
    struct remrep_texture_blanket* rep = (struct remrep_texture_blanket*)message;
    if(rep->success) {
      DBG("Ack: texture_blanket\n");
    }
    else {
      printf("!!! TEXTURE_BLANKET FAILED. This may cause subsequent failures and incorrect rendering\n");
    }
    break;
  }
  case REMREP_GET_TEX_SURFACE: {
    struct remrep_get_tex_surface* rep = (struct remrep_get_tex_surface*)message;
    if(rep->success) {
      DBG("Ack: get_tex_surface\n");
    }
    else {
      printf("!!! GET_TEX_SURFACE FAILED. This may cause subsequent failures and incorrect rendering\n");
    }
    break;
  }
  case REMREP_BUFFER_SET_DATA: {
    struct remrep_buffer_set_data* rep = (struct remrep_buffer_set_data*)message;
    if(rep->success) {
      DBG("Ack: buffer_set_data\n");
    }
    else {
      printf("!!! BUFFER_SET_DATA FAILED. This may cause subsequent failures and incorrect rendering\n");
    }
    break;
  }
  default:
    printf("BUG! Check async result handed opcode %s\n", optotext(opcode));
  }
    

}

void discard_asynchronous_messages(int block) {

  uint32_t bufsize = ((uint32_t*)singleton_screen->rx_buffer)[0];
  uint32_t* readoff = &(((uint32_t*)singleton_screen->rx_buffer)[1]);
  uint32_t* writeoff = &(((uint32_t*)singleton_screen->rx_buffer)[2]);
  char* readbuf = (char*)&(((uint32_t*)singleton_screen->rx_buffer)[3]);

  if(block) {
    DBG("Discarding async messages (blocking)\n");
  }
  else {
    DBG("Discarding async messages (non-blocking)\n");
  }
  // WARNING HERE: This method assumes that no async failure notification
  // is bigger than the ring. If one were, we would wait fruitlessly until
  // the entire thing were present before deleting it from the ring.

  /* If block is set, we should wait until a synchronous message arrives.
     If block is not set, we should return as soon as we're short of data. */

  int need_more_bytes = 0;

  while(1) {

    struct message_header head;

    if(need_more_bytes) {
      if(block) {
	DBG("Insufficient data, but invoked blocking: sleeping\n");
	wait_for_rx_not_empty();
	need_more_bytes = 0;
	DBG("Woken\n");
      }
      else {
	DBG("Non-blocking discard_async_messages: exiting\n");
	return;
      }
    }

    if(drain_events(singleton_screen->socketfd) == -1) {
      printf("Socket error draining events in discard_async\n");
      return;
    }

    int bytes_peeked = recvsome(readoff, writeoff, bufsize, readbuf, (char*)&head, sizeof(struct message_header));

    if(bytes_peeked < sizeof(struct message_header)) {
      DBG("  Less than a header available (%d bytes); exiting\n", bytes_peeked);
      need_more_bytes = 1;
      continue;
    }

    if(!message_is_asynchronous(head.opcode)) {
      DBG("  Next message (%s) not asynchronous: exiting\n", optotext(head.opcode));
      return;
    }

    int bytes_available = read_avail(readoff, writeoff, bufsize);

    if(bytes_available < head.length) {
      DBG("  Message had length %d but only %d available: exiting\n", head.length, bytes_available);
      need_more_bytes = 1;
      continue;
    }

    char* message_proper = malloc(head.length);
    if(!message_proper) {
      printf("OOM checking for asynchronous failures\n");
      return;
    }

    int bytes_read = recvsome(readoff, writeoff, bufsize, readbuf, message_proper, head.length);

    if(bytes_read < head.length) {
      printf("BUG! read_avail returned %d, yet we could only actually read %d bytes\n", bytes_available, bytes_read);
      free(message_proper);
      return;
    }

    DBG("  Got a message: checking its result\n");

    check_async_result(message_proper, head.opcode);

    free(message_proper);

    discard_ring_bytes(readoff, bufsize, head.length);

  }

}

void wait_for_synchronous_message(void) {

  /* This is like check_for_asynchronous_failures, but we must block
     until a synchronous message comes along */

  discard_asynchronous_messages(1);

}

void check_for_asynchronous_failures(void) {

  discard_asynchronous_messages(0);

}

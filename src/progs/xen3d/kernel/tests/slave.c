
#include <xenctrl.h>
#include <xs.h>
#include <stdio.h>
#include <string.h>

#include <stdint.h>

#include <asm/ioctl.h>

#include <xen/xen.h>
#include <xen/sys/gntmem.h>
#include <xen/grant_table.h>

#include <sys/mman.h>

#include <stdlib.h>
#include <fcntl.h>

int nextquotient = 4;
char nextletter = 'A';

int process(char* buf, int length) {

  if(nextquotient == 4)
    nextquotient = 2;
  else if(nextquotient == 2)
    nextquotient = 1;
  else
    nextquotient = 4;

  int bytestowrite = length/nextquotient;
  int i;

  for(i=0;i<bytestowrite;i++) {
    buf[i] = nextletter++;
    if(nextletter > 'z')
      nextletter = 'A';
  }

  return bytestowrite;

}
  

int main(int argc, char* argv) {

  int i;
  int xceh = xc_evtchn_open();

  if(xceh == -1) {
    printf("Failed\n");
    return 1;
  }
  
  evtchn_port_or_error_t port = xc_evtchn_bind_unbound_port(xceh, 0);
  
  if(port == -1) {
    printf("Failed to bind port\n");
    return 1;
  }
  else {
    printf("Got an event channel: %d\n", port);
  }
  
  int fd = open("/dev/gntmem", O_RDWR);
  int ret;
  
  if(fd == -1) {
    printf("open\n");
    return 1;
  }

  ret = ioctl(fd, IOCTL_GNTMEM_SET_DOMAIN, 0);

  if(ret == -1) {
    printf("set-domain\n");
    return 1;
  }

  ret = ioctl(fd, IOCTL_GNTMEM_SET_SIZE, 4);

  if(ret == -1) {
    printf("set-size\n");
    return 1;
  }

  grant_ref_t grants[4];
  memset(grants, 0, sizeof(grant_ref_t) * 4);

  ret = ioctl(fd, IOCTL_GNTMEM_GET_GRANTS, grants);

  if(ret == -1) {
    printf("get-grants\n");
    return 1;
  }

  void* address = (void*)-1;
  address = mmap(0, 4096 * 4, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  if(address == (void*)-1) {
    printf("mmap\n");
    return 1;
  }

  printf("Got shared section at %p\n", address);

  // Init the ring buffer before we announce it to the remote
  uint32_t* initdata = (uint32_t*)address;
  uint32_t buffersize = 4096 * 4 - (sizeof(uint32_t) * 3);
  initdata[0] = buffersize;
  // total buffer size
  initdata[1] = 0;
  initdata[2] = 0;
  // Next byte to read and next-to-write respectively.
  // Equality signals the buffer is empty, write = read - 1 signals it is full

  struct xs_handle* xsh = xs_domain_open();

  if(!xsh) {
    printf("Failed to open Xenstore\n");
    return 1;
  }

  unsigned int length = 0;
  char* mydomid_string = xs_read(xsh, XBT_NULL, "domid", &length);

  if(mydomid_string == 0) {
    printf("Failed to read domid\n");
    return 1;
  }

  int mydomid;
  printf("Got my domain string: %s, length %d\n", mydomid_string, length);
  sscanf(mydomid_string, "%d", &mydomid);

  printf("I appear to be in domain %d\n", mydomid);

  char localstore[1024];
  char remotestore[1024];

#define RELATIVE_LOCAL "device/xen3d"

  sprintf(localstore, "/local/domain/%d/device/xen3d", mydomid);
  sprintf(remotestore, "/local/domain/0/backend/xen3d/requests/%d/frontend", mydomid);

  printf("Going to create a request at %s pointing to %s\n", remotestore, localstore);

  xs_rm(xsh, XBT_NULL, localstore);
  bool success = xs_mkdir(xsh, XBT_NULL, localstore);

  if(!success) {
    printf("Failed to create %s\n", localstore);
    return 1;
  }

  char eventchanpath[1024];

  sprintf(eventchanpath, "%s/event-channel", localstore);

  success = xs_mkdir(xsh, XBT_NULL, eventchanpath);
  if(!success) {
    printf("Failed to create %s\n", eventchanpath);
    return 1;
  }

  char eventchandata[20];
  sprintf(eventchandata, "%d", port);
  int eventchandatalength;
  eventchandatalength = strlen(eventchandata);

  success = xs_write(xsh, XBT_NULL, eventchanpath, eventchandata, eventchandatalength);
  if(!success) {
    printf("Failed to write %s to %s\n", eventchandata, eventchanpath);
    return 1;
  }

  for(i = 0; i < 4; i++) {
    sprintf(eventchanpath, "%s/pages/%d", localstore, i);
    sprintf(eventchandata, "%u", grants[i]);
    eventchandatalength = strlen(eventchandata);
    success = xs_write(xsh, XBT_NULL, eventchanpath, eventchandata, eventchandatalength);
    if(!success) {
      printf("Failed to write %s to %s\n", eventchandata, eventchanpath);
      return 1;
    }
  }

  xs_transaction_t rqtrans = xs_transaction_start(xsh);

  if(!rqtrans) {
    printf("Failed to start a transaction\n");
    return 1;
  }

  success = xs_mkdir(xsh, rqtrans, remotestore);
  if(!success) {
    printf("Failed to create %s\n", remotestore);
    return 1;
  }

  int local_path_length = strlen(RELATIVE_LOCAL);

  success = xs_write(xsh, rqtrans, remotestore, RELATIVE_LOCAL, local_path_length);
  if(!success) {
    printf("Failed to write to %s\n", remotestore);
    return 1;
  }

  success = xs_transaction_end(xsh, rqtrans, 0);
  if(!success) {
    printf("Transaction failed\n");
    return 1;
  }

  printf("Successfully created %s with value %s\n", remotestore, RELATIVE_LOCAL);

  for (i = 0; i < 4; i++)
    printf("Grant-ref for page %d is %d\n", i, grants[i]);

  char* data = (char*)(&(((uint32_t*)address)[3]));
  uint32_t* readoff = &(((uint32_t*)address)[1]);
  uint32_t* writeoff = &(((uint32_t*)address)[2]);

  while(1) {

    while(1) {
      int space = 0;
      uint32_t limit = *readoff;
      uint32_t base = *writeoff;
      int writewrapsbuffer;

      //      printf("On going around, read = %u, write = %u\n", limit, base);

      if(limit > base) {
	space = (limit - base) - 1;
	writewrapsbuffer = 0;
      }
      else {
	space = base - limit;
	space = (buffersize - space) - 1;
	// Care! > rather than >= because if it == buffersize, the write does
	// not wrap, but the pointer does
	if((base + space) > buffersize)
	  writewrapsbuffer = 1;
	else
	  writewrapsbuffer = 0;
      }
      
      //      printf("Determined this means we can write %d bytes at most\n", space);
      //      if(writewrapsbuffer)
      //printf("The write may wrap the ring\n");

      if(space == 0) {
	//printf("Looks like the buffer is full: let's wait\n");
	break;
      }

      // Okay, we have (space) bytes available; write into that.
      int byteswritten;

      if(!writewrapsbuffer) {
	// The buffer is already flat: write in place
	//	printf("Writing to offset %u in place\n", base);
	byteswritten = process(&(data[base]), space);
	//	printf("%d bytes were written\n", byteswritten);
      }
      else {
	// Our process function can only write flat memory; have it write into
	// a temporary buffer then copy that into the ring appropriately.
	char* temp = malloc(space);
	//	printf("Writing to a temporary of size %d\n", space);
	byteswritten = process(temp, space);
	//	printf("%d bytes were written\n");

	int bytestoend = buffersize - base;
	int bytestocopy = byteswritten < bytestoend ? byteswritten : bytestoend;
	memcpy(&(data[base]), temp, bytestocopy);
	//	printf("Copied %d bytes to ring offset %u\n", bytestocopy, base);
	if(bytestocopy == bytestoend) {
	  //	  printf("Copied %d bytes to ring offset 0\n", byteswritten - bytestoend);
	  memcpy(data, &(temp[bytestoend]), byteswritten - bytestoend);
	}

	free(temp);
      }

      uint32_t newwriteoff = base + byteswritten;
      if(newwriteoff >= buffersize)
	newwriteoff -= buffersize;

      *writeoff = newwriteoff;

      int ret = xc_evtchn_notify(xceh, port);
      if(ret == -1) {
	printf("Failed to notify not empty\n");
      }
    }

    // We currently see the buffer as full. Wait for it to empty.
    //    printf("*** Buffer full (read = %u, write = %u), waiting\n", *readoff, *writeoff);


    evtchn_port_or_error_t notified = xc_evtchn_pending(xceh);
    xc_evtchn_unmask(xceh, notified);
    //    printf("Notified\n");

  }

  return 0;

}

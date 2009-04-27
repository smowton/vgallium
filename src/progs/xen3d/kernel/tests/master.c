
#include <sys/mman.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <xenctrl.h>
#include <xs.h>

char last = 'z';
int lastquotient = 4;
long int totalbytesread = 0;

int process(char* buffer, int length) {

  if(lastquotient == 4)
    lastquotient = 1;
  else if(lastquotient == 2)
    lastquotient = 4;
  else if(lastquotient == 1)
    lastquotient = 2;

  int bytestoread = length/lastquotient;

  int i;
  for(i = 0; i < bytestoread; i++) {
    if(last == 'z')
      last = 'A';
    else
      last++;
    
    if(buffer[i] != last) {
      printf("Error! Expected %c, got %c at index %d\n", last, buffer[i], i);
      printf("Dump of the full buffer being read:\n");
      int j;
      for(j = 0; j < bytestoread; j++) {
	printf("%c", buffer[j]);
      }
      exit(1);
    }
    else {
      totalbytesread++;
      if((totalbytesread % 1000000)== 0) {
	printf("%d bytes read without error\n", totalbytesread);
      }
    }
  }
  
  return bytestoread;

}

int main(int argc, char* argv) {

  uint32_t domains[4];
  uint32_t grants[4];

  int i;

  struct xs_handle* xsh = xs_daemon_open();

  if(!xsh) {
    printf("Failed to open Xenstore\n");
    return 1;
  }
  
  xs_rm(xsh, XBT_NULL, "/local/domain/0/backend/xen3d");

  struct xs_permissions perms;

#define WATCH_NODE "/local/domain/0/backend/xen3d/requests"

  bool ret = xs_mkdir(xsh, XBT_NULL, WATCH_NODE);
  if(!ret) {
    printf("Couldn't mkdir\n");
    return 1;
  }

  // Make it world-writable
  perms.id = 0;
  perms.perms = XS_PERM_WRITE;
  ret = xs_set_permissions(xsh, XBT_NULL, WATCH_NODE, &perms, 1);

  int ret2 = xs_watch(xsh, WATCH_NODE, "watch");
  if(ret2 == -1) {
    printf("Failed to watch the watchnode\n");
    return 1;
  }

  evtchn_port_or_error_t evtid = -1;
  int domid = -1;

  while(evtid == -1) {

    int len;
    char** watch_paths = xs_read_watch(xsh, &len);
    if(!watch_paths) {
      printf("Failed to get watch paths\n");
      return 1;
    }

    for(i = 0; i < len; i += 2) {

      char* path = watch_paths[i];
      char* token = watch_paths[i+1];
      printf("%s changed (token %s)\n", path, token);

      char test = '\0';

      sscanf(path, WATCH_NODE"/%d/fronten%c", &domid, &test);

      if(domid >= 0 && test == 'd') {
	char* frontend = xs_read(xsh, XBT_NULL, watch_paths[i], NULL);
	if(!frontend) {
	  printf("Failed to read from %s\n", watch_paths[i]);
	  return 1;
	}
	else {

	  int i;

	  printf("Frontend discovered for domain %d, giving its relative path as %s\n", domid, frontend);
	  char evtchnpath[1024];
	  sprintf(evtchnpath, "/local/domain/%d/%s/event-channel", domid, frontend);
	  char* evtstring = xs_read(xsh, XBT_NULL, evtchnpath, NULL);
	  if(!evtstring) {
	    printf("Failed to read from %s\n", evtchnpath);
	    return 1;
	  }
	  sscanf(evtstring, "%d", &evtid);
	  printf("Remote specifies an event channel with ID %d\n", evtid);

	  free(evtstring);

	  for(i=0; i < 4; i++) {
	    sprintf(evtchnpath, "/local/domain/%d/%s/pages/%d", domid, frontend, i);
	    evtstring = xs_read(xsh, XBT_NULL, evtchnpath, NULL);
	    if(!evtstring) {
	      printf("Failed to read from %s\n", evtchnpath);
	      return 1;
	    }

	    domains[i] = (uint32_t)domid;
	    sscanf(evtstring, "%u", &(grants[i]));
	  }

	}
      }
    }
  }

  int xceh = xc_evtchn_open();

  if(xceh == -1) {
    printf("Failed to open evtchn\n");
    return 1;
  }
  else {
    printf("Opened evtchn: got handle %d\n");
  }

  evtchn_port_or_error_t port = xc_evtchn_bind_interdomain(xceh, domid, evtid);

  if(port == -1) {
    printf("Failed to connect\n");
    return 1;
  }
  else {
    printf("Got a port: %d, connected to domain %u / port %u\n", port, domid, evtid);
  }

  int handle = xc_gnttab_open();

  if(handle == -1) {
    printf("open\n");
    return 1;
  }

  void* mem = xc_gnttab_map_grant_refs(handle, 4, domains, grants, PROT_READ | PROT_WRITE);

  if(mem == (void*)-1) {
    printf("map\n");
    return 1;
  }

  uint32_t buffersize = *((uint32_t*)mem);
  uint32_t* readoff = &(((uint32_t*)mem)[1]);
  uint32_t* writeoff = &(((uint32_t*)mem)[2]);
  char* bufferstart = (char*)&(((uint32_t*)mem)[3]);

  while(1) {

    while(*readoff != *writeoff) {

      uint32_t limit = *writeoff;
      uint32_t base = *readoff;
      int bytestoread;
      int wrapsbuffer;

      //      printf("On going around, read-offset is %u and write-offset is %u\n", base, limit);

      if(limit > base) {
	bytestoread = limit - base;
	wrapsbuffer = 0;
      }
      else {
	bytestoread = buffersize - (base - limit);
	wrapsbuffer = 1;
      }

      //      printf("Determined that means we have %d bytes to read\n", bytestoread);

      if(!wrapsbuffer) {
	//printf("Reading from %u in-place: %d bytes to read\n", base, bytestoread);
	// Flat already: process in place
	int bytesread = process(&(bufferstart[base]), bytestoread);
	//printf("Read %d bytes\n", bytesread);
	*readoff += bytesread;
      }
      else {
	char* flattened = malloc(bytestoread);
	int bytestoend = buffersize - base;

	//printf("Flattening\n");

	memcpy(flattened, &(bufferstart[base]), bytestoend);
	//printf("Read %d bytes from ring index %u\n", bytestoend, base);
	memcpy(&(flattened[bytestoend]), bufferstart, bytestoread - bytestoend);
        //printf("Read %d bytes from ring index 0\n", bytestoread - bytestoend);

	int bytesread = process(flattened, bytestoread);
	//printf("Processing read %d bytes\n", bytesread);

	free(flattened);

	uint32_t newreadoff = base + bytesread;
	if(newreadoff >= buffersize)
	  newreadoff -= buffersize;

	*readoff = newreadoff;
      }

      int err = xc_evtchn_notify(xceh, port);
      if(err == -1) {
	printf("Failed to notify-not-full; errno = %d\n", errno);
      }

    }

    //printf("*** Ring empty: waiting (read: %u, write: %u)\n", *readoff, *writeoff);

    // Now we see the ring buffer as empty, and the event channel is certainly
    // unmasked. Wait for a not-empty signal.

    evtchn_port_or_error_t notified = xc_evtchn_pending(xceh);
    //    printf("Notified on port %d!\n", notified);
    xc_evtchn_unmask(xceh, notified);	

    //    printf("Notified\n");

  }

  return 0;

}


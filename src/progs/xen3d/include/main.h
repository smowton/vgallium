
#ifndef XEN3D_MAIN_H
#define XEN3D_MAIN_H

#include "remote_messages.h"
#include "pipe/p_compiler.h"

void* allocate_message_memory(size_t);

struct client_list_entry;
struct client_context;

int send_message(struct client_list_entry*, void*);

void destroy_context(struct client_context* ctx);

struct tcp_receiver_state {

  int fd;

};

struct xidc_receiver_state {

  int gntdev_handle;
  int return_gntdev_handle;
  void* buffer;
  void* return_buffer;
  int evtid;
  int return_evtid;
  uint32_t grants[4];
  uint32_t return_grants[4];

  int evtchnhandle;

};

struct receiver {

  char* command;
  char* recv_current;
  char* target;

  char* partial_send;
  int partial_send_length;
  int bytes_sent;

  int (*process)(char*, int, void*);
  void* process_arg;
  int (*receive)(void*, char*, int);
  void* receive_arg;
  int (*send)(void*, char*, int);
  int (*send_blocking)(void*, char*, int);
  void* send_arg;
  int max_updates_before_yield;
    
};

#define ALLOC_MESSAGE(varname, typename) \
  struct remrep_##typename* varname = CALLOC_STRUCT(remrep_##typename); \
  varname->base.length = sizeof(struct remrep_##typename);

#endif

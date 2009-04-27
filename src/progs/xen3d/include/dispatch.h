#ifndef XEN3D_DISPATCH_H
#define XEN3D_DISPATCH_H

#include "client.h"
#include "pipe/p_compiler.h"

int get_message_size(char* message);

int dispatch_message(struct client_list_entry*, char* message);

#endif

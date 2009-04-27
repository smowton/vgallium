
#ifndef REMOTE_COMMS_H
#define REMOTE_COMMS_H

#define ALLOC_OUT_MESSAGE(req, name) struct remreq_##req* name = (struct remreq_##req*)allocate_message_memory(sizeof(struct remreq_##req));

#define QUEUE_AND_WAIT(msg, req, reply_name) enqueue_message(msg); struct remrep_##req* reply_name = (struct remrep_##req*)get_message();

void* allocate_message_memory(size_t);

void enqueue_message(void*);

struct message_header* get_message();

void free_message(void*);

#endif

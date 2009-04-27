
// Dumb responder: accepts a connection on port 1812 and accepts a screen
// creation request, responding by giving a handle.

#include "remote_messages.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

int main(int argc, char** argv) {

    int fd = socket(PF_INET, SOCK_STREAM, 0);
    if(fd == -1) {
	printf("socket() failed\n");
	exit(1);
    }
    
    struct sockaddr_in bind_address;
    memset(&bind_address, 0, sizeof(struct sockaddr_in));
    bind_address.sin_family = AF_INET;
    bind_address.sin_port = htons(1812);
    
    int ret = bind(fd, &bind_address, sizeof(struct sockaddr_in));
    if(ret == -1) {
	printf("bind() failed\n");
	exit(1);
    }
    
    ret = listen(fd, 1);
    if(ret == -1) {
	printf("listen() failed\n");
	exit(1);
    }

    struct sockaddr_in client_address;
    socklen_t length = sizeof(struct sockaddr_in);
    memset(&client_address, 0, sizeof(struct sockaddr_in));
    int connfd = accept(fd, &client_address, &length);
    
    if(ret == -1) {
	printf("accept() failed\n");
	exit(1);
    }
    
    printf("Connected: remote is %s\n", inet_ntoa(client_address.sin_addr));
    
    struct remreq_create_screen message1;
    
    ssize_t recvd = recv(connfd, &message1, sizeof(struct remreq_create_screen), MSG_WAITALL);
    
    printf("Received %d bytes\n", recvd);
    
    if(recvd == -1) {
	printf("Socket error receiving (error %d)\n", errno);
	exit(1);
    }
    if(recvd < sizeof(struct remreq_create_screen)) {
	printf("Error: received less than the length of a create-screen request (%d bytes)\n", recvd);
	exit(1);
    }
    
    if(message1.base.opcode != REMREQ_CREATE_SCREEN) {
	printf("Error: received a message, but it was not a screen creation request (had opcode %u)\n", message1.base.opcode);
	exit(1);
    }
    
    struct remrep_create_screen reply;
    reply.base.opcode = REMREP_CREATE_SCREEN;
    reply.base.length = sizeof(struct remrep_create_screen);
    reply.handle = 1;
    
    ssize_t sent = send(connfd, &reply, sizeof(struct remrep_create_screen), MSG_WAITALL);
    
    if(sent < sizeof(struct remrep_create_screen)) {
	printf("Error: could not send entire create-screen reply\n");
	exit(1);
    }
    
    printf("Done\n");
    close(fd);
    
    return 0;
    
}
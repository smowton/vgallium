
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <X11/Xlib.h>
#include <X11/extensions/xen3d_extproto.h>
#include <X11/Xregion.h>

int main(int argc, char** argv) {

    int rxsock, datasock;

    if(!(rxsock = socket(PF_INET, SOCK_STREAM, 0))) {
    
	printf("Socket failed\n");
	exit(0);
	
    }
    
    struct sockaddr_in listen_address;
    
    memset(&listen_address, 0, sizeof(struct sockaddr_in));
    inet_aton("127.0.0.1", &listen_address.sin_addr);
    listen_address.sin_port = htons(1815);
    
    if(bind(rxsock, (struct sockaddr*)&listen_address, sizeof(struct sockaddr_in))) {
    
	printf("Bind failed\n");
	exit(0);
	
    }
    
    if(listen(rxsock, 5)) {
    
	printf("Listen failed\n");
	exit(0);
	
    }
    
    struct sockaddr_in remote_address;
    socklen_t length = sizeof(struct sockaddr_in);
    
    datasock = accept(rxsock, (struct sockaddr*)&remote_address, &length);
    
    if(datasock < 0) {
	printf("Accept failed\n");
	exit(0);
    }
    
    XVMGLWindowingCommand* command = malloc(sizeof(XVMGLWindowingCommand));
    char* recv_current = (char*)command;
    char* target = recv_current + sizeof(XVMGLWindowingCommand);
    
    int err;
    
    while((err = recv(datasock, recv_current, target - recv_current, 0)) > 0) {
    
	    recv_current += err;
	    
	    if(recv_current == target) {
	    
		int bytes_received = recv_current - (char*)command;
		int total_bytes_required = sizeof(XVMGLWindowingCommand) + (sizeof(BoxRec) * ntohl(command->length));
		
		if(bytes_received == total_bytes_required) {
		
		    printf("Got a window update:\n");
		    printf("Screen-ID: %u\n", ntohl(command->screenid));
		    printf("GL Window: %u\n", ntohl(command->glWindow));
		    printf("Location: (%u, %u)\n", ntohl(command->x), ntohl(command->y));
		    printf("Dimension: %ux%u\n", ntohl(command->width), ntohl(command->height));
		    
		    int boxes = ntohl(command->length);
		    
		    printf("Cliprects: %d\n", boxes);
		    
		    int i;
		    for(i = 0; i < boxes; i++) {
		    
			BoxPtr thisbox = &(((BoxPtr)(((char*)command) + sizeof(XVMGLWindowingCommand)))[i]);
			
			printf("Rect %d: (%hu, %hu) to (%hu, %hu)\n", i, thisbox->x1, thisbox->y1, thisbox->x2, thisbox->y2);
			
		    }
		
		    recv_current = (char*)command;
		    target = ((char*)command) + sizeof(XVMGLWindowingCommand);
		    
		}
		else {
		
		    command = realloc(command, total_bytes_required);
		    recv_current = ((char*)command) + bytes_received;
		    
		    target = ((char*)command) + total_bytes_required;
    
		}
	
	    }
	    
    }

    if(err == 0) {
	
        printf("Orderly close by X server: exiting\n");
        return 0;
    
    }
    else if(err == -1) {
	
        printf("Socket error communicating with X server: exiting\n");
        return 1;
	    
    }
    
}

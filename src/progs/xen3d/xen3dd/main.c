#include <xenctrl.h>
#include <xs.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>

#include "utils.h"

#ifdef DEBUG
#define DBG(format, args...) dolog(LOG_DEBUG, format, ## args);
#else
#define DBG(format, args...)
#endif

#define SOCKET_PATH "/var/run/xen3dd-socket"

struct client_list_entry {
  int fd;
  evtchn_port_or_error_t rx_event;
  uint32_t rx_grants[4];
  evtchn_port_or_error_t tx_event;
  uint32_t tx_grants[4];
  char *xenstore_path;
  struct client_list_entry *next;
};

struct startup_message {
  uint32_t rx_grants[4];
  uint32_t tx_grants[4];
  char client_type;
};

static int mydomid;
static int lastrq = 0;

static void setnonblocking(int sock)
{
  int opts;

  opts = fcntl(sock,F_GETFL);
  if (opts < 0) {
    dolog(LOG_ERR, "Failed to set socket non-blocking\n");
    exit(1);
  }

  opts = (opts | O_NONBLOCK);
  if (fcntl(sock,F_SETFL,opts) < 0) {
    dolog(LOG_ERR, "Failed to set socket non-blocking\n");
    exit(1);
  }

  return;
}

static int receive_all(int fd, void *data, int length)
{
  int sent = 0;

  while (sent < length) {
    int this_recv = recv(fd, (void*)((char*)data + sent), length - sent, 0);
    if (this_recv == 0) {
      dolog(LOG_ERR, "Unexpected EOF whilst receiving a message\n");
      return -1;
    } else if (this_recv == -1) {
      if (errno != EAGAIN) {
	dolog(LOG_ERR, "Socket error whilst receiving a message; errno = %d\n", errno);
	return -1;
      }
    } else
      sent += this_recv;
  }

  return 0;
}

static void handle_new_connection(int listenfd, int evtchnhandle,
				  struct xs_handle *xenstorehandle,
				  struct client_list_entry **listhead)
{
  char *localstore = malloc(1024);
  char localrela[1024];
  char remotestore[1024];

  struct client_list_entry *new_entry = 0;

  localstore[0] = '\0';
  localrela[0] = '\0';
  remotestore[0] = '\0';

  struct sockaddr_un client_address;
  socklen_t length = sizeof(struct sockaddr_un);
  memset(&client_address, 0, sizeof(struct sockaddr_un));

  DBG("Handling new connection\n");
  int connfd = accept(listenfd, (struct sockaddr*)&client_address, &length);
  if (connfd == -1) {
    dolog(LOG_ERR, "accept() failed\n");
    goto out;
  }

  setnonblocking(connfd);

  struct startup_message initdata;

  int ret = receive_all(connfd, &initdata, sizeof(struct startup_message));
  if (ret < 0) {
    dolog(LOG_ERR, "Failed to receive init information; dropping a connection\n");
    goto out;
  }

  evtchn_port_or_error_t rx_event = xc_evtchn_bind_unbound_port(evtchnhandle, 0);
  evtchn_port_or_error_t tx_event = xc_evtchn_bind_unbound_port(evtchnhandle, 0);
  if (rx_event == -1 || tx_event == -1) {
    dolog(LOG_ERR, "Failed to bind port\n");
    goto out;
  }

  if ((snprintf(localstore, 1024, "/local/domain/%d/device/xen3d/%d",
		mydomid, lastrq) > 1024) ||
      (snprintf(localrela, 1024, "device/xen3d/%d", lastrq) > 1024) ||
      (snprintf(remotestore, 1024,
		"/local/domain/0/backend/xen3d/requests/%d/%d/frontend",
		mydomid, lastrq) > 1024)) {
    dolog(LOG_ERR, "One or more of my Xenstore strings exceeded 1024 chars in length\n");
    goto out;
  }

  DBG("Going to create a request at %s pointing to %s\n", remotestore, localstore);

  xs_rm(xenstorehandle, XBT_NULL, localstore);
  bool success = xs_mkdir(xenstorehandle, XBT_NULL, localstore);
  if(!success) {
    dolog(LOG_ERR, "Failed to create %s\n", localstore);
    goto out;
  }

  char eventchanpath[1024];
  char eventchandata[20];
  int eventchandatalength;

  sprintf(eventchanpath, "%s/event-channel", localstore);
  success = xs_mkdir(xenstorehandle, XBT_NULL, eventchanpath);
  if (!success) {
    dolog(LOG_ERR, "Failed to create %s\n", eventchanpath);
    goto out;
  }

  sprintf(eventchandata, "%d", tx_event);
  eventchandatalength = strlen(eventchandata);

  success = xs_write(xenstorehandle, XBT_NULL, eventchanpath, eventchandata,
		     eventchandatalength);
  if (!success) {
    dolog(LOG_ERR, "Failed to write %s to %s\n", eventchandata, eventchanpath);
    goto out;
  }

  sprintf(eventchanpath, "%s/return-event-channel", localstore);

  success = xs_mkdir(xenstorehandle, XBT_NULL, eventchanpath);
  if(!success) {
    dolog(LOG_ERR, "Failed to create %s\n", eventchanpath);
    goto out;
  }

  sprintf(eventchandata, "%d", rx_event);
  eventchandatalength = strlen(eventchandata);

  success = xs_write(xenstorehandle, XBT_NULL, eventchanpath, eventchandata,
		     eventchandatalength);
  if (!success) {
    dolog(LOG_ERR, "Failed to write %s to %s\n", eventchandata, eventchanpath);
    goto out;
  }

  struct {
    char *dirname;
    uint32_t *grants;
  } pagelocs[2] = {
    {.dirname = "pages", .grants = initdata.tx_grants},
    {.dirname = "return-pages", .grants = initdata.rx_grants}
  };

  for (int j = 0; j < 2; j++) {
    for (int i = 0; i < 4; i++) {
      sprintf(eventchanpath, "%s/%s/%d", localstore, pagelocs[j].dirname, i);
      sprintf(eventchandata, "%u", pagelocs[j].grants[i]);
      eventchandatalength = strlen(eventchandata);
      success = xs_write(xenstorehandle, XBT_NULL, eventchanpath, eventchandata, eventchandatalength);
      if (!success) {
	dolog(LOG_ERR, "Failed to write %s to %s\n", eventchandata, eventchanpath);
	goto out;
      }
    }
  }

  sprintf(eventchanpath, "%s/type", localstore);

  char typedata[4];

  sprintf(typedata, "%d", (int)initdata.client_type);
  success = xs_write(xenstorehandle, XBT_NULL, eventchanpath, typedata, 1);
  if(!success) {
    dolog(LOG_ERR, "Failed to write %s to %s\n", eventchandata, eventchanpath);
    goto out;
  }

  xs_transaction_t rqtrans = xs_transaction_start(xenstorehandle);
  if (!rqtrans) {
    dolog(LOG_ERR, "Failed to start a transaction\n");
    goto out;
  }

  success = xs_mkdir(xenstorehandle, rqtrans, remotestore);
  if (!success) {
    dolog(LOG_ERR, "Failed to create %s\n", remotestore);
    goto out;
  }

  int local_path_length = strlen(localrela);

  success = xs_write(xenstorehandle, rqtrans, remotestore, localrela, local_path_length);
  if (!success) {
    dolog(LOG_ERR, "Failed to write to %s\n", remotestore);
    goto out;
  }

  success = xs_transaction_end(xenstorehandle, rqtrans, 0);
  if (!success) {
    dolog(LOG_ERR, "Transaction failed\n");
    goto out;
  }

  DBG("Successfully created %s with value %s\n", remotestore, localrela);

  // Now make a list entry describing the new client
  new_entry = malloc(sizeof(struct client_list_entry));
  if (!new_entry) {
    dolog(LOG_ERR, "Failed to malloc a client list entry\n");
    goto out;
  }

  for(int k = 0; k < 4; k++) {
    new_entry->rx_grants[k] = initdata.rx_grants[k];
    new_entry->tx_grants[k] = initdata.tx_grants[k];
  }

  new_entry->rx_event = rx_event;
  new_entry->tx_event = tx_event;
  new_entry->xenstore_path = localstore;
  new_entry->fd = connfd;
  new_entry->next = 0;

  // Finally finally, add it to the list
  while (*listhead)
    listhead = &((*listhead)->next);

  *listhead = new_entry;

  lastrq++;

  return;

 out:
  if (connfd != -1)
    close(connfd);
  if (rx_event != -1)
    xc_evtchn_unbind(evtchnhandle, rx_event);
  if (tx_event != -1)
    xc_evtchn_unbind(evtchnhandle, tx_event);
  if (localstore[0] != '\0')
    xs_rm(xenstorehandle, XBT_NULL, localstore);
  free(new_entry);
  free(localstore);

  return;
}

static int handle_incoming(struct client_list_entry *client, int evtchnhandle)
{
  char next;
  int ret;

  while ((ret = recv(client->fd, &next, 1, 0)) > 0) {
    DBG("Received char %c\n", next);
    if (next == 'R') {
      DBG("Notifying rx event\n");
      if (xc_evtchn_notify(evtchnhandle, client->rx_event) == -1) {
	dolog(LOG_ERR, "Failed to notify event %d\n", client->rx_event);
	return 0;
      }
    } else if (next == 'T') {
      DBG("Notifying tx event\n");
      if (xc_evtchn_notify(evtchnhandle, client->tx_event) == -1) {
	dolog(LOG_ERR, "Failed to notify event %d\n", client->tx_event);
	return 0;
      }
    }
  }

  if (ret != -1 || errno != EAGAIN) {
    DBG("After receiving, had ret = %d, errno = %d; reporting failure\n", ret, errno);
    return 0;
  } else
    return 1;
}

static void destroy_client(struct client_list_entry *client, int evtchnhandle,
			   struct xs_handle *xenstorehandle, struct client_list_entry **listhead)
{
  // First, break down the client's entry in Xenstore.
  DBG("Breaking down client with Xenstore path %s\n", client->xenstore_path);

  if (client->xenstore_path)
    xs_rm(xenstorehandle, XBT_NULL, client->xenstore_path);
  if (client->rx_event != -1)
    xc_evtchn_unbind(evtchnhandle, client->rx_event);
  if (client->tx_event != -1)
    xc_evtchn_unbind(evtchnhandle, client->tx_event);
  if (client->fd != -1)
    close(client->fd);

  // That should adequately signal the remote that we've died.

  struct client_list_entry *previous = 0;
  while (listhead) {
    if (*listhead == client) {
      *listhead = (*listhead)->next;
      break;
    }
    listhead = &((*listhead)->next);
  }
}

static void notify_client(struct client_list_entry *client, char notify)
{
  int oldflags = fcntl(client->fd, F_GETFL);

  int blockingflags = oldflags & ~O_NONBLOCK;

  fcntl(client->fd, F_SETFL, blockingflags);

  int err;
  if ((err = send(client->fd, &notify, 1, 0)) < 1)
    dolog(LOG_ERR, "Failed to send an event on to a client with fd %d: return %d / errno %d\n",
	  client->fd, err, errno);

  fcntl(client->fd, F_SETFL, oldflags);
}

static void handle_event(int evtchnhandle, struct client_list_entry *clients)
{
  evtchn_port_or_error_t event = xc_evtchn_pending(evtchnhandle);
  DBG("Got an event from port %d\n", event);
  xc_evtchn_unmask(evtchnhandle, event);

  while (clients) {
    if (clients->rx_event == event) {
      DBG("Matched a client's rx-event, sending 'R'\n");
      notify_client(clients, 'R');
    } else if (clients->tx_event == event) {
      DBG("Matched a client's tx-event, sending 'T'\n");
      notify_client(clients, 'T');
    }
    clients = clients->next;
  }
}

static void main_loop(int listenfd, int evtchnhandle, struct xs_handle *xenstorehandle)
{
  struct client_list_entry *client_list_head = NULL;

  int evtchnfd = xc_evtchn_fd(evtchnhandle);
  DBG("Got an fd corresponding to handle %d: %d\n", evtchnhandle, evtchnfd);

  while(1) {
    // Select on the listen fd, all clients and incoming events
    fd_set rx_set;
    int highestfd = listenfd;
    FD_ZERO(&rx_set);

    FD_SET(listenfd, &rx_set);
    DBG("Select: added unix-domain fd %d\n", listenfd);

    FD_SET(evtchnfd, &rx_set);
    DBG("Select: added evtchn fd %d\n", evtchnfd);
    if (evtchnfd > highestfd)
      highestfd = evtchnfd;

    struct client_list_entry *next_client = client_list_head;

    while(next_client) {
      FD_SET(next_client->fd, &rx_set);
      DBG("Select: added client fd %d\n", next_client->fd);
      if (next_client->fd > highestfd)
	highestfd = next_client->fd;

      next_client = next_client->next;
    }

    DBG("Selecting...\n");
    int ret = select(highestfd + 1, &rx_set, NULL, NULL, NULL);

    DBG("Select returned %d active FDs\n", ret);

    if (FD_ISSET(listenfd, &rx_set)) {
      DBG("Found unix-domain fd active\n");
      handle_new_connection(listenfd, evtchnhandle, xenstorehandle,
			    &client_list_head);
    }

    if (FD_ISSET(evtchnfd, &rx_set)) {
      DBG("Found evtchn FD active\n");
      handle_event(evtchnhandle, client_list_head);
    }

    next_client = client_list_head;

    while (next_client) {

      int failure = 0;

      if(FD_ISSET(next_client->fd, &rx_set)) {
	
	DBG("Found client fd %d active\n", next_client->fd);	
	int success = handle_incoming(next_client, evtchnhandle);

	if(!success) {
	  struct client_list_entry* to_destroy = next_client;
	  destroy_client(next_client, evtchnhandle, xenstorehandle, &client_list_head);
	  next_client = next_client->next;
	  free(to_destroy);
	  failure = 1;
	}

      }

      if(!failure) {
	next_client = next_client->next;
      }
      failure = 0;

    }
  }
}

static int init_unix_socket()
{
  int fd = socket(PF_UNIX, SOCK_STREAM, 0);
  if(fd == -1) {
    dolog(LOG_ERR, "socket() failed (Unix domain)\n");
    return -1;
  }

  struct sockaddr_un un_bind_address;
  memset(&un_bind_address, 0, sizeof(struct sockaddr_un));
  un_bind_address.sun_family = AF_UNIX;

  unlink(SOCKET_PATH);
  strcpy(un_bind_address.sun_path,SOCKET_PATH);

  int ret = bind(fd, (struct sockaddr*)&un_bind_address,
		 sizeof(un_bind_address.sun_family) + strlen(un_bind_address.sun_path));
  if (ret == -1) {
    dolog(LOG_ERR, "bind() failed (Unix domain)\n");
    return -1;
  }

  ret = listen(fd, 1);
  if (ret == -1) {
    dolog(LOG_ERR, "listen() failed (Unix domain)\n");
    return -1;
  }

  setnonblocking(fd);

  return fd;
}

static void daemonise()
{
  pid_t pid, sid;

  pid = fork();
  if (pid == -1) {
    dolog(LOG_ERR, "Failed to fork\n");
    exit(1);
  } else if(pid > 0) {
    // This is the parent process: exit.
    exit(0);
  }

  umask(0);

  sid = setsid();
  if (sid < 0) {
    dolog(LOG_ERR, "Failed to setsid()\n");
    exit(1);
  }

  if (chdir("/") < 0) {
    dolog(LOG_ERR, "Failed to chdir\n");
    exit(1);
  }

  freopen("/dev/null", "r", stdin);
  //  if(!freopen("/var/log/xendd", "w", stdout))
  freopen("/dev/null", "w", stdout);
  freopen("/dev/null", "w", stderr);
}

int main(int argc, char **argv)
{
  openlog("xen3dd", LOG_CONS, LOG_DAEMON);

  if (argc == 1)
    daemonise();

  signal(SIGPIPE, SIG_IGN);

  int listenfd = init_unix_socket();
  DBG("Opened " SOCKET_PATH ", got fd %d\n", listenfd);
  if (listenfd == -1) {
    dolog(LOG_ERR, "Failed to open Unix socket\n");
    goto _exit;
  }

  int evtchnhandle = xc_evtchn_open();
  DBG("Opened the event channel driver, got handle %d\n", evtchnhandle);
  if (evtchnhandle == -1) {
    dolog(LOG_ERR, "Failed to open event-channel device\n");
    goto _exit;
  }

  struct xs_handle *xenstorehandle = xs_domain_open();
  DBG("Opened the Xenstore device, got handle %p\n", xenstorehandle);

  if (!xenstorehandle) {
    dolog(LOG_ERR, "Failed to open xenstore\n");
    goto _exit;
  } else {
    unsigned int length = 0;
    char *mydomid_string = xs_read(xenstorehandle, XBT_NULL, "domid", &length);

    if (mydomid_string == 0) {
      dolog(LOG_ERR, "Failed to read domid\n");
      goto _exit;
    } else {
      sscanf(mydomid_string, "%d", &mydomid);
      DBG("I appear to be in domain %d\n", mydomid);
      free(mydomid_string);

      char mystore[1024];
      if (snprintf(mystore, 1024, "/local/domain/%d/device/xen3d",
		   mydomid) >= 1024) {
	dolog(LOG_ERR, "My xenstore path is too long for my 1024 char buffer\n");
	goto _exit;
      } else
	xs_rm(xenstorehandle, XBT_NULL, mystore);
    }
  }

  main_loop(listenfd, evtchnhandle, xenstorehandle);

 _exit:
  dolog(LOG_ERR, "Failed during init\n");
  closelog();
  exit(1);
}

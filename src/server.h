#ifndef SERVER_H
#define SERVER_H

#include "socket.h"
#include "chat_helpers.h"


extern pid_t server_pid;
extern int server_num;
extern int server_port;
// Tracks client ID should be given 
extern int client_id;
extern int client_connected;

int start_server(int port);
int setup_server_socket(struct listen_socket *s, int port);
int accept_connection(int fd, struct client_socket **clients);
void clean_exit(struct listen_socket s, struct client_socket *clients, int exit_status);
int run_server(int port);

struct server_socket {
    int sock_fd;
    char buf[BUF_SIZE];
    int inbuf;
};

// close the server. Return 0 if server is succesfully closed, else -1. 
int cleanup_server();


#endif // SERVER_H;
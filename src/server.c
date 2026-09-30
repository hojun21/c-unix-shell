#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>
#include <sys/select.h>
#include "socket.h"
#include "server.h"
#include "io_helpers.h"
#include "chat_helpers.h"
#include "builtins.h"

// pid of server
pid_t server_pid = -1;
// port number of server 
int server_port = -1;
// number of server running
int server_num = 0;

int client_id = 0;
int client_connected = 0;

// start server with given port number
// return -1 on failure, 0 on success
int start_server(int port){
    
    pid_t pid = fork();
    server_pid = pid;

    if(pid < 0){
        return -1;
    } else if (pid == 0){
        // child process run the server
        int run = run_server(port);

        if(run == -1){
            return -1;
        }

        exit(0);
    }

    return 0;
    
}

int accept_connection(int fd, struct client_socket **clients) {
    
    // peer stores client's address
    struct sockaddr_in peer;
    unsigned int peer_len = sizeof(peer);
    peer.sin_family = AF_INET;
    
    int num_clients = 0;
    struct client_socket *curr = *clients; // head of client

    while (curr != NULL && curr->next != NULL) {
        curr = curr->next;
        num_clients++;
    }

    int client_fd = accept(fd, (struct sockaddr *)&peer, &peer_len);
    
    if (client_fd < 0) {
        perror("server: accept");
        close(fd);
        exit(1);
    }

    // Create new client 
    struct client_socket *newclient = malloc(sizeof(struct client_socket));
    newclient->sock_fd = client_fd;
    newclient->inbuf = newclient->state = 0;
    newclient->username = NULL;
    newclient->next = NULL;
    memset(newclient->buf, 0, BUF_SIZE);

    if (*clients == NULL) {
        *clients = newclient;
    }
    else {
        curr->next = newclient;
    }

    client_connected++;

    return client_fd;
}

// Clean all clients when server is down. 
void clean_exit(struct listen_socket s, struct client_socket *clients, int exit_status) {
    struct client_socket *tmp;
    while (clients) {
        tmp = clients;
        close(tmp->sock_fd);
        clients = clients->next;
        free(tmp->username);
        free(tmp);
    }
    close(s.sock_fd);
    free(s.addr);
    exit(exit_status);
}

int run_server(int port) {
    // This line causes stdout not to be buffered.

    client_connected = 0;
    
    setbuf(stdout, NULL);

    signal(SIGPIPE, SIG_IGN);

    // Linked list of clients
    struct client_socket *clients = NULL;
    
    struct listen_socket s;

    int result = setup_server_socket(&s, port);

    // ERROR : failed to set up server socket. 
    if(result == -1){
        return -1;
    }
    
    int exit_status = 0;
    
    int max_fd = s.sock_fd;

    fd_set all_fds, listen_fds;
    
    FD_ZERO(&all_fds);
    FD_SET(s.sock_fd, &all_fds);

    do {
        listen_fds = all_fds;
        
        int nready = select(max_fd + 1, &listen_fds, NULL, NULL, NULL);
        
        if (nready == -1) {
            if (errno == EINTR) continue;
            display_error("ERROR: Failed to select.\n","");
            return -1;
            // exit_status = 1;
            //break;
        }

        /* 
         * If a new client is connecting, create new
         * client_sock struct and add to clients linked list.
         */
        if (FD_ISSET(s.sock_fd, &listen_fds)) {
            int client_fd = accept_connection(s.sock_fd, &clients);
            if (client_fd < 0) {
                display_error("Failed to accept new connection.\n", "");
                continue;
            }
            if (client_fd > max_fd) {
                max_fd = client_fd;
            }
            FD_SET(client_fd, &all_fds);
            // display_message("Accepted connection\n");
        }

        /*
         * Accept incoming messages from clients,
         * and send to all other connected clients.
         */
        struct client_socket *curr = clients;
        while (curr) {

            
            if (!FD_ISSET(curr->sock_fd, &listen_fds)) {
                curr = curr->next;
                continue;
            }
            int client_closed = read_from_client(curr);
            
            // If error encountered when receiving data
            if (client_closed == -1) {
                client_closed = 1; // Disconnect the client
            }
            
            // If received at least one complete message
            // and client is newly connected: Get username
            if (client_closed == 0 && curr->username == NULL) {
                if (set_username(curr, ++client_id) == -1) {
                    display_error("ERROR: Failed to assign USER ID.\n", "");
                    client_closed = 1; // Disconnect the client
                }
            }
                
            char *msg;
            // Loop through buffer to get complete message(s)
            while (client_closed == 0 && !get_message(&msg, curr->buf, &(curr->inbuf))) {

                if (strncmp(msg, "\\connected", 11) == 0){
                    char connect_msg[BUF_SIZE];
                    //snprintf(connect_msg, BUF_SIZE, "%d clients are in chat.", client_id);
                    snprintf(connect_msg, BUF_SIZE, "%d", client_connected);
                    char buf[BUF_SIZE];

                    buf[0] = '\0';
                    strncat(buf, connect_msg, BUF_SIZE - 1);
                    strncat(buf, "\n", BUF_SIZE - 1);
                    
                    write_buf_to_client(curr, buf, strlen(buf)); // No need to handle \r\n. 
                    
                    free(msg);
                    continue; 
                }

               
                // Prepare to send message to users 
                char write_buf[BUF_SIZE];
                write_buf[0] = '\0';
                strncat(write_buf, curr->username, MAX_NAME);
                strncat(write_buf, " ", MAX_NAME);
                strncat(write_buf, msg, MAX_USER_MSG);

                int data_len = strlen(write_buf);
                
                struct client_socket *dest_c = clients;
                while (dest_c) {
                    // if (dest_c != curr) {
                        int ret = write_buf_to_client(dest_c, write_buf, data_len);
                        
                        if (ret != 0) {
                            // failed to sent message 
                            char fail_msg[BUF_SIZE];
                            snprintf(fail_msg, BUF_SIZE, "%s has failed to send message.\n", curr->username);
                            display_message(fail_msg);
                            

                            // User disconnects 
                            if (ret == 2) {


                                close(dest_c->sock_fd);
                                FD_CLR(dest_c->sock_fd, &all_fds);
                                
                                if(remove_client(&dest_c, &clients) != 0){
                                    display_error("ERROR: Failed to disconnect client\n", "");
                                }
                                
                                
                                continue;
                            }
                        }
                    // }
                    dest_c = dest_c->next;
                }
                // After sending message, print it to server shell
                char server_msg[BUF_SIZE];
                snprintf(server_msg, BUF_SIZE, "%s: %s\n", curr->username, msg);
                display_message(server_msg);
                free(msg);
            }
            
            if (client_closed == 1) { // Client disconnected
                // Note: Never reduces max_fd when client disconnects
                FD_CLR(curr->sock_fd, &all_fds);
                close(curr->sock_fd);
                client_connected--;

                assert(remove_client(&curr, &clients) == 0); // If this fails we have a bug
                
            }
            else {
                curr = curr->next;
            }
        }
    } while(1); // to be added 
    
    clean_exit(s, clients, exit_status);
    return 0;
}


int cleanup_server() {
    
    if (server_num > 0 && server_pid > 0) {
        
        if (kill(server_pid, SIGTERM) == -1) {
            display_error("ERROR: Failed to kill server.\n", "");
            return -1;
        }

        char buf[BUF_SIZE]; 
        snprintf(buf, BUF_SIZE, "Server %d is closed successfully.\n", server_port); 
        display_message(buf);
    
        server_pid = -1;
        server_port = -1;
        server_num = 0;
        return 0;
    } 
    
    return 0;
}
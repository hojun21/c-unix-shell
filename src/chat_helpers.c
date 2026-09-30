#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "socket.h"
#include "chat_helpers.h"
#include "server.h"
#include "io_helpers.h"


// Set user name with given id. Return 0 on Sucess, -1 on failure
int set_username(struct client_socket *curr, int id) {
    
    char username[MAX_NAME];

    snprintf(username, sizeof(username), "client%d", id);

    curr->username = strdup(username);
    if (curr->username == NULL) {
        display_error("ERROR: Failed to allocate memory for clients", "");
        return -1; 
    }

    return 0;

}

/* 
 * Read incoming bytes from client.
 * 
 * Return -1 if read error or maximum message size is exceeded.
 * Return 0 upon receipt of CRLF-terminated message.
 * Return 1 if client socket has been closed.
 * Return 2 upon receipt of partial (non-CRLF-terminated) message.
 */
int read_from_client(struct client_socket *curr) {
    // read_from_socket : 1 -> valid, -1 -> error
    return read_from_socket(curr->sock_fd, curr->buf, &(curr->inbuf));
}

int write_buf_to_client(struct client_socket *c, char *buf, int len) {
    
    char send_buf[BUF_SIZE];
    memcpy(send_buf, buf, len);
    send_buf[len] = '\r';
    send_buf[len+1] = '\n';

    int result = write_to_socket(c->sock_fd, send_buf, len + 2);
    
    // error 
    if(result == 0){
        return 0;
    } else if (result == -1){
        if(errno == EPIPE || errno == ECONNRESET){
            return 2;
        }
        return 1;
    }
    return 1;   
}

/* 
 * Remove client from list. Return 0 on success, 1 on failure.
 * Update curr pointer to the new node at the index of the removed node.
 * Update clients pointer if head node was removed.
 */
int remove_client(struct client_socket **curr, struct client_socket **clients) {
    // clients : linked list of clients
    // curr : client that want to remove from linkedlist

    if (*clients == NULL || *curr == NULL) {
        return 1; 
    }
    struct client_socket *remove = *curr;

    // if head has to be deleted, remove head 
    if(*clients == remove){
        *clients = remove->next;
        *curr = remove->next;
        free(remove->username);
        free(remove);
        return 0;
    } else {
        // if should be deleted client is middle of linked list
        struct client_socket *cur = *clients;

        while(cur->next != NULL){
            if(cur->next == remove){
                cur->next = cur->next->next;
                *curr = remove->next;
                free(remove->username);
                free(remove);
                return 0;
            }
            cur = cur->next;
        }
    }

    return 1; // Couldn't find the client in the list, or empty list
}
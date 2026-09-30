#ifndef CHAT_HELPERS_H
#define CHAT_HELPERS_H

struct client_socket {
    int sock_fd;
    int state;
    char *username;
    char buf[BUF_SIZE];
    int inbuf;
    struct client_socket *next;
};

int set_username(struct client_socket *curr, int id);

int read_from_client(struct client_socket *curr);

int write_buf_to_client(struct client_socket *c, char *buf, int len);

int remove_client(struct client_socket **curr, struct client_socket **clients);

#endif // CHAT_HELPERS_H;
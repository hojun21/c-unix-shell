    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <unistd.h>
    #include <sys/socket.h>
    #include <errno.h>
    #include <arpa/inet.h>     
    #include <netdb.h>     
    #include <netinet/in.h>   
    #include "io_helpers.h"
    #include "socket.h"



    // set up the server socket.
    int setup_server_socket(struct listen_socket *s, int port){
        if(!(s->addr = malloc(sizeof(struct sockaddr_in)))) {
            display_error("ERROR: Failed to allocate memory", "");
            return -1;
        }
        // Allow sockets across machines.
        s->addr->sin_family = AF_INET;
        // The port the process will listen on.
        s->addr->sin_port = htons(port);
        // Clear this field; sin_zero is used for padding for the struct.
        memset(&(s->addr->sin_zero), 0, 8);
        // Listen on all network interfaces.
        s->addr->sin_addr.s_addr = INADDR_ANY;

        s->sock_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (s->sock_fd < 0) {
            display_error("ERROR: Failed to set up socket", "");
            exit(1);
        }

        // Make sure we can reuse the port immediately after the
        // server terminates. Avoids the "address in use" error
        int on = 1;
        int status = setsockopt(s->sock_fd, SOL_SOCKET, SO_REUSEADDR,
            (const char *) &on, sizeof(on));
        if (status < 0) {
            // display_error("ERROR: Failed to set socket FD", "");
            return -1;
        }

        // Bind the selected port to the socket.
        if (bind(s->sock_fd, (struct sockaddr *)s->addr, sizeof(*(s->addr))) < 0) {
            // display_error("ERROR: Failed to bind","");
            free(s->addr);
            close(s->sock_fd);
            return -1;
        }

        // Announce willingness to accept connections on this socket.
        if (listen(s->sock_fd, MAX_BACKLOG) < 0) {
            // display_error("ERROR: Failed to listen", "");
            free(s->addr);
            close(s->sock_fd);
            return -1;
        }

        return 0;
    }

    int read_from_socket(int sock_fd, char *buf, int *inbuf) {
        // sock_fd = file descriptor
        // buf = buffer
        // inbuf = # of characters read 
        int bytes_read = read(sock_fd, buf + *inbuf, BUF_SIZE - *inbuf - 1);

        // no characters are read -> socket is closed 
        if(bytes_read == 0){
            return 1;
        } else if (bytes_read < 0){
            // read error 
            return -1;
        }

        // update total number of bytes read
        *inbuf += bytes_read;  

        if(*inbuf >= BUF_SIZE){
            return -1;
        }

        for(int i = 0; i < *inbuf - 1; i++){
            if(buf[i] == '\r' && buf[i+1] == '\n'){
                return 0;  
            }
        }
        return 2;
    }

    int get_message(char **dst, char *src, int *inbuf) {

        // Implement the find_network_newline() function
        // before implementing this function.
        int newline = find_network_newline(src, *inbuf);

        if(newline == -1){
            return 1; // error if not found
        }

        *dst = malloc(sizeof(char) * (newline - 1));
        if(*dst == NULL){
        }

        memcpy(*dst, src, newline - 2);
        (*dst)[newline - 2] = '\0';

        int remain_bytes = *inbuf - newline;

        if (remain_bytes > 0) {
            memmove(src, src + newline, remain_bytes);
        }

        *inbuf -= newline;
        // memmove(src, src + newline, *inbuf);

        return 0;
    }


    /*
    * Search the first inbuf characters of buf for a network newline (\r\n).
    * Return one plus the index of the '\n' of the first network newline,
    * or -1 if no network newline is found.
    * Definitely do not use strchr or other string functions to search here. (Why not?)
    */
    int find_network_newline(const char *buf, int inbuf) {

        for(int i = 0; i < inbuf - 1; i++){
            if(buf[i] == '\r' && buf[i + 1] == '\n'){
                return i + 2;
            }
        }
        // not found
        return -1;
    }

    int write_to_socket(int sock_fd, char *buf, int len) {
        
        int bytes_sent;
        int total_bytes_sent = 0;
    
        while(total_bytes_sent < len){

            bytes_sent = write(sock_fd, buf + total_bytes_sent, len - total_bytes_sent);

            if(bytes_sent <= 0){
                display_error("ERROR: Failed to write", "");
                return -1;  
            }
            // successfully sent    
            total_bytes_sent += bytes_sent;
    
        }
        return 0;
    }


    // Connect to server. Return sockfd if successfully connects, -1 if error. 
    int connect_to_server(int port, char *hostname){

        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd < 0) {
            display_error("ERROR: Failed to set up socket", "");
            return -1;
        }
    
        struct addrinfo hints;
        struct addrinfo *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;

        // Port already provided. 
        int status = getaddrinfo(hostname, NULL, &hints, &res);
        if (status != 0) {
            display_error("ERROR: Invalid hostname", "");
            close(sockfd);
            return -1;
        }
        // set up server
        struct sockaddr_in addr;
        addr.sin_family = PF_INET;
        addr.sin_port = htons(port);
        memset(&(addr.sin_zero), 0, 8);

        struct sockaddr_in *res_addr = (struct sockaddr_in *)res->ai_addr;
        addr.sin_addr = res_addr->sin_addr;
        
        freeaddrinfo(res); 

        // Request connection to server.
        if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
            display_error("ERROR: Failed to connect", "");
            close(sockfd);
            return -1;
        }

        return sockfd;

    }

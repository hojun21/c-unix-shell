#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include "variables.h"
#include "builtins.h"
#include "io_helpers.h"
#include <ctype.h> 
#include <stdlib.h>
#include <dirent.h>
#include <sys/types.h>
#include "commands.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include "server.h"
#include <netdb.h>
#include <arpa/inet.h>
#include "socket.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <errno.h>


#define BUFFERSIZE 4096

volatile sig_atomic_t disconnect_client = 0;

void client_signal_handler(int sig) {
    if (sig == SIGINT) {
      
        disconnect_client = 1;
    }
}

// ====== Command execution =====

/* Return: index of builtin or -1 if cmd doesn't match a builtin
*/
bn_ptr check_builtin(const char *cmd) {
    // argument : first token of input
    ssize_t cmd_num = 0;
    while (cmd_num < BUILTINS_COUNT &&
        strncmp(BUILTINS[cmd_num], cmd, MAX_STR_LEN) != 0) {
        cmd_num += 1;
    }
    return BUILTINS_FN[cmd_num];
}


// ===== Builtins =====

/* Prereq: tokens is a NULL terminated sequence of strings.
* Return 0 on success and -1 on error ... but there are no errors on echo. 
*/
ssize_t bn_echo(char **tokens) {
    
    char result[MAX_STR_LEN + 1]; 
    memset(result, '\0', MAX_STR_LEN);
    size_t remaining = MAX_STR_LEN;
    int index = 1;

    // return -1 if something goes wrong.. 
    if (tokens[0] == NULL) {
        return -1;
    }

    // There is an input 
    while (tokens[index] != NULL){
        if(remaining < 1){
            break;
        }
        // TODO:
        // Implement the echo command
        char* value_to_append = expand_variables(tokens[index]);
        strncat(result, value_to_append, remaining);
        remaining -= strlen(value_to_append);
        free(value_to_append);

        index++;

        if(tokens[index] != NULL){
            strncat(result, " ", remaining);
            remaining--;
        }
    }

    result[MAX_STR_LEN] = '\0';
    
    display_message(result);
    display_message("\n");
    return 0;
}   

/*
This helper method recursively prints all the directories and files in the given path. 
Precondition : valid path, depth is given.
*/
void recursive_ls(char path[], ls_info *info, int remain_depth){
    
    // if rec && !depth, traverse until the end 
    if(info->has_depth == 0){
        remain_depth = -1;
    }
    else if(remain_depth == 0){
        return;
    }
    DIR *dir = opendir(path);
    if (dir == NULL) {
        return;
    }
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {

        if (info->has_substring) {

            // if it contains substring, print it
            if (strstr(entry->d_name, info->substring) != NULL) {
                display_message(entry->d_name);
                display_message("\n");
                

                // recursively traverse except "." and ".." to prevent overflow
                if (strncmp(entry->d_name, ".", 2) != 0 &&
                    strncmp(entry->d_name, "..", 3) != 0){
                    char full_path[strlen(entry->d_name) + strlen(path) + 2];
                    snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
                    recursive_ls(full_path, info, remain_depth - 1);
                }
                
            }
            // if there direcotry is not substring, traverse files under the same directory
            else{
                if(strncmp(entry->d_name, ".", 2) != 0 &&
                strncmp(entry->d_name, "..", 3) != 0){
                    char full_path[strlen(entry->d_name) + strlen(path) + 2];
                    snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
                    recursive_ls(full_path, info, remain_depth - 1);
                }
            }
        }
        
        // no substring 
        else{
            display_message(entry->d_name);
            display_message("\n");
            if(strncmp(entry->d_name, ".", 2) != 0 &&
            strncmp(entry->d_name, "..", 3) != 0){
                    char full_path[strlen(entry->d_name) + strlen(path) + 2];
                    snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
                    recursive_ls(full_path, info, remain_depth - 1);
            }
        }
        
    }
    closedir(dir);
}

/* Prereq: tokens is a NULL terminated sequence of strings.
* Print all directories & files with given option
* Return 0 on success and -1 on error
*/
ssize_t bn_ls(char **tokens){
    ls_info info = parse_ls(tokens);
    
    if(info.has_path > 1){
        display_error("ERROR: Too many arguments: ls takes a single path", "");
        return -1;
    }

    if(info.valid != 1){
        return -1;
    } else if(info.has_rec == 0 && info.has_depth == 1){
        return -1;
    }

    DIR *dir = opendir(info.path);
    
    if(dir == NULL){
        display_error("ERROR: Invalid path", "");
        return -1;
    }

    // when --rec is not given 
    struct dirent *entry;

    if(info.has_rec){
        recursive_ls(info.path, &info, info.depth);
    }
    else{
        // if there is substring given, print only directories with substring
        if(info.has_substring == 1){
            while ((entry = readdir(dir)) != NULL) {

                if(strstr(entry->d_name, info.substring) != NULL){
                    display_message(entry->d_name);
                    display_message("\n");
                }
            }
        }
        // if there is no substring, print every directories
        else{
            while ((entry = readdir(dir)) != NULL) {

                    display_message(entry->d_name);
                    display_message("\n");
                }
        }
    }

    closedir(dir);
    return 0;
}

// iterate ls commands and parse options 
ls_info parse_ls(char **tokens){
    
    ls_info info = {
        .has_path = 0,
        .path = ".",
        .has_rec = 0,
        .has_substring = 0,
        .has_depth = 0,
        .depth = -1,   
        .substring = NULL,
        .valid = 1 // 1 if valid, else 0. 
    };

    for(int i = 1; tokens[i] != NULL; i++){
        
        // if --f is read 
        if(strcmp(tokens[i], "--f") == 0){
            
            // check if next option is substring 
            if (tokens[i + 1] == NULL || strlen(tokens[i + 1]) == 0 ||
                strncmp(tokens[i + 1], "--f", 3) == 0 || 
                strncmp(tokens[i + 1], "--d", 3) == 0 || 
                strncmp(tokens[i + 1], "--rec", 5) == 0) {
                    info.valid = 0;
                    return info;
            } else {

                // check if differnet substring already exists 
                if(info.has_substring == 1 && strcmp(info.substring, tokens[i + 1]) != 0){
                    info.valid = 0;
                    return info;
                }

                info.substring = tokens[i + 1];
                info.has_substring = 1;
                i++;
                continue;
            }
        }

        // if --d is read 
        else if(strcmp(tokens[i], "--d") == 0){

            // check if next option is positive integer
            if(tokens[i + 1] == NULL){
                info.valid = 0;
                return info;
            }
            
            // check if depth is given as positive integer 
            for(int j = 0; tokens[i+1][j] != '\0'; j++) {
                if(!isdigit(tokens[i+1][j])) {
                    info.valid = 0;
                    return info; 
                }
            }
            info.depth = atoi(tokens[i + 1]);
            info.has_depth = 1;
            i++; 
            continue;
        }


        // if --rec is read 
        else if(strcmp(tokens[i], "--rec") == 0){
            info.has_rec = 1;
            continue;
        }

        // if path is read 
        else {

            // if two paths are read
            if(info.has_path > 0){
                info.has_path += 1;
                info.valid = 0;
                return info;
            }
            
            // variable expansion for file 
            if (strchr(tokens[i], '$') != NULL && strlen(tokens[i]) >= 2) {
                // Expand the variable in the path
                char *expanded_path = expand_variables(tokens[i]);
                strncpy(info.path, expanded_path, MAX_STR_LEN);
                info.path[MAX_STR_LEN] = '\0'; 
                free(expanded_path);
                
            } else{
                strncpy(info.path, tokens[i], MAX_STR_LEN); 
                info.path[MAX_STR_LEN] = '\0';
            }

            // when path is read for first time
            info.has_path++;
        }
    }
    return info;
}

/*
* cd to different directories. Return 0 on success, -1 on failure.
*/
ssize_t bn_cd(char **tokens){
    if (tokens[1] == NULL) {
        display_error("ERROR: No input source provided", "");
        return -1;
    }

    if (tokens[2] != NULL){
        display_error("ERROR: Too many arguments: cd takes a single path", "");
        return -1;
    }  

    // initial path
    char *path = tokens[1];
    char final_path[MAX_STR_LEN + 1] = "";

    if (path[0] == '/') {
        strcat(final_path, "/");
    }

    // if there is $ in path, expand the variable 
    if (strchr(path, '$') != NULL) {

        char* array = strtok(path, "/");

        while(array != NULL){
            
            if(strchr(array, '$') != NULL){
                char *expanded_path = expand_variables(array);
                if(strlen(expanded_path) == 3 && strncmp(expanded_path, "...", 3) == 0){
                    strncat(final_path, "../.." ,MAX_STR_LEN - strlen(final_path));
                }   
                else if(strlen(expanded_path) == 4 && strncmp(expanded_path, "....", 4) == 0){
                    strncat(final_path, "../../.." ,MAX_STR_LEN - strlen(final_path));
                }
                else{
                    strncat(final_path, expanded_path, MAX_STR_LEN - strlen(final_path));
                }
                free(expanded_path);
            }
            // Handle "...", "...." cases 
            else{
                if(strlen(array) == 3 && strncmp(array, "...", 3) == 0){
                    strncat(final_path, "../.." ,MAX_STR_LEN - strlen(final_path));
                }   
                else if(strlen(array) == 4 && strncmp(array, "....", 4) == 0){
                    strncat(final_path, "../../.." ,MAX_STR_LEN - strlen(final_path));
                }
                else{
                    strncat(final_path, array, MAX_STR_LEN - strlen(final_path));
                }
            }

            array = strtok(NULL, "/");
            if (array != NULL) {
                strcat(final_path, "/");
            }
        }
    } else {
        char* array = strtok(path, "/");

        while(array != NULL){
            if(strlen(array) == 3 && strncmp(array, "...", 3) == 0){
                strncat(final_path, "../.." ,MAX_STR_LEN - strlen(final_path));
            }   
            else if(strlen(array) == 4 && strncmp(array, "....", 4) == 0){
                strncat(final_path, "../../.." ,MAX_STR_LEN - strlen(final_path));
            }
            else{
                strncat(final_path, array, MAX_STR_LEN - strlen(final_path));
            }

            array = strtok(NULL, "/");
            if (array != NULL) {
                strcat(final_path, "/");
            }
        }
        
    }
    if (chdir(final_path) == -1) {
        display_error("ERROR: Invalid path", "");
        return -1;
    }

    return 0;

}


/*
* Cat files and print out the content if given path is valid. 
* Return 0 on success, -1 on failure.
*/
ssize_t bn_cat(char** tokens){

    char *path = tokens[1];

    if(path == NULL){

        // if path is not given, check input from pipe 
        char buffer[BUFFERSIZE];
        
        ssize_t bytes_read = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);

        if(bytes_read > 0){
            buffer[bytes_read] = '\0';
            display_message(buffer);
        }   
        
        else {
            display_error("ERROR: No input source provided", "");
            return -1;
        }  
        
        // check additional files to read 
        while((bytes_read = read(STDIN_FILENO, buffer, sizeof(buffer) - 1)) > 0){
            buffer[bytes_read] = '\0';
            display_message(buffer);
        }
        return 0;
    }
    
    if(tokens[2] != NULL){
        display_error("ERROR: Too many arguments: cat takes a single file", "");
        return -1;
    }

    // variable expansion for file 

    char final_path[BUFFERSIZE + 1] = "";
    if (strchr(tokens[1], '$') != NULL && strlen(tokens[1]) >= 2) {
        char *copy_path = strdup(tokens[1]);
        char *array = strtok(copy_path, "/");

        while(array != NULL){
            if(strchr(array, '$') != NULL){
                char *expanded_path = expand_variables(array);
                strncat(final_path, expanded_path, BUFFERSIZE - strlen(final_path));
                free(expanded_path);
            }
            else{
                strncat(final_path, array, BUFFERSIZE - strlen(final_path));
            }

            array = strtok(NULL, "/");
            if(array != NULL){
                strncat(final_path, "/", BUFFERSIZE - strlen(final_path));
            }
        }
        final_path[BUFFERSIZE] = '\0';
        free(copy_path);

    } else {
        strncpy(final_path, tokens[1], BUFFERSIZE);
        final_path[BUFFERSIZE] = '\0';
    }

    FILE *file = fopen(final_path, "r");
    if(file == NULL){
        display_error("ERROR: Cannot open file", "");
        return -1;
    }

    char buffer[BUFFERSIZE];
    while(fgets(buffer, sizeof(buffer), file) != NULL){
        display_message(buffer);
    }

    fclose(file);
    return 0;

}

/*
 * Read given file and print word count, character count, and new line count. 
 * Return 0 on success, -1 on failure.
 */
ssize_t bn_wc(char **tokens){
    char *path = tokens[1];
    int char_count = 0;
    int word_count = 0;
    int line_count = 0;
    int in_word = 0; // 1 if in word, 0 if not in word.
    // char buffer[BUFFERSIZE];
    int ch;
    FILE *file;

    if(path == NULL){

        file = fdopen(STDIN_FILENO, "r");
        if (file == NULL) {
            display_error("ERROR: Cannot read from stdin", "");
            return -1;
        }
    }

    else {

        if(tokens[2] != NULL){
            display_error("ERROR: Too many arguments: cat takes a single file", "");
            return -1;
        }

        // variable expansion for file 

        char final_path[BUFFERSIZE + 1] = "";
        if (strchr(tokens[1], '$') != NULL && strlen(tokens[1]) >= 2) {
            char *copy_path = strdup(tokens[1]);
            char *array = strtok(copy_path, "/");

            while(array != NULL){
                if(strchr(array, '$') != NULL){
                    char *expanded_path = expand_variables(array);
                    strncat(final_path, expanded_path, BUFFERSIZE - strlen(final_path));
                    free(expanded_path);
                }
                else{
                    strncat(final_path, array, BUFFERSIZE - strlen(final_path));
                }

                array = strtok(NULL, "/");
                if(array != NULL){
                    strncat(final_path, "/", BUFFERSIZE - strlen(final_path));
                }
            }
            final_path[BUFFERSIZE] = '\0';
            free(copy_path);

        } else {
            strncpy(final_path, tokens[1], BUFFERSIZE);
            final_path[BUFFERSIZE] = '\0';
        }

        file = fopen(final_path, "r");
        if(file == NULL){
            display_error("ERROR: Cannot open file", "");
            return -1;
        }
    }
    while((ch = fgetc(file)) != EOF) {
        char_count++;

        if(ch == '\n'){
            line_count++;
            in_word = 0;
        }
        else if (isspace(ch)) {
            in_word = 0;
        }
        // ch is character
        else if(in_word == 0){
            in_word = 1;
            word_count++;
        }
    }

    char result[BUFFERSIZE];
    snprintf(result, sizeof(result), "word count %d\n", word_count);
    display_message(result);

    snprintf(result, sizeof(result), "character count %d\n", char_count);
    display_message(result);

    snprintf(result, sizeof(result), "newline count %d\n", line_count);
    display_message(result);

    return 0;
}

// shows all the processes running in the background. 
// return 1 on success, -1 on unexpected error. 
ssize_t bn_ps(){

    // if not one yet
    for(int i = 0; available_indices[i] != 1 && i < MAX_PROCESS; i++){
        // iterate until 0 or 2 (running or finished running)
        if(bg_processes[i].is_running == 1){
            char *command_first = strtok(strdup(bg_processes[i].command), " ");
            if (command_first == NULL) {
                command_first = bg_processes[i].command;
            }
        
            char message[MAX_STR_LEN * 2 + 1];
            snprintf(message, MAX_STR_LEN * 2, "%s %d", command_first, bg_processes[i].pid);
            message[MAX_STR_LEN * 2] = '\0';
            display_message(message);
            display_message("\n");

            free(command_first);
            continue;
        }
    }
    return 0;
}


// kill the processor with given signal number
// return 0 on success, -1 on failure. 
ssize_t bn_kill(char **tokens){
    pid_t pid;
    if (tokens[1] == NULL) {
        return -1;
    } else {
        pid = atoi(tokens[1]);
    }

    int signo = SIGTERM; // 15
    // Valid signal number is between 1 and 64

    if (tokens[2] != NULL){

        if(tokens[2][0] == '$'){
            char *expanded_signal = expand_variables(tokens[2]);

            if (strlen(expanded_signal) == 0) {
                display_error("ERROR: Invalid signal specified", "");
                free(expanded_signal);
                return -1;
            }

            signo = atoi(expanded_signal);
            free(expanded_signal);

        } else {
            signo = atoi(tokens[2]);
        }
        
        if (signo < 1 || signo > 64) {
            display_error("ERROR: Invalid signal specified", "");
            return -1;
        }
    }

    if (kill(pid, signo) == -1) {
        display_error("ERROR: The process does not exist", "");
        return -1;
    } else {
        // successfully killed 
        for(int i = 0; i < MAX_PROCESS && available_indices[i] != 1; i ++){
            if(bg_processes[i].pid == pid && signo != 2){
                available_indices[i] = 3; 
                break;
            }
        }
    }
    
    return 0;
}

// Start server with given port number. Return 0 on Success, -1 on failure. 
ssize_t bn_start_server(char **tokens){

    if (tokens[1] == NULL) {
        display_error("ERROR: No port provided", "");
        return -1;
    }

    int port = atoi(tokens[1]);

    // valid port number : 1024 ~ 65535
    if (port < 1024 || port > 65535) {
        display_error("ERROR: Invalid port number", "");
        return -1;
    }

    // failed to start_server
    if(start_server(port) == -1){
        display_error("ERROR: Failed to start server", "");
        return -1; 
    }

    server_port = port;
    server_num = 1;
    // successfully started server
    return 0;
}

ssize_t bn_close_server(char **tokens){
    // No need to use tokens 
    (void)tokens;
    int result = cleanup_server();

    return result; 
}

// Send message to given port and host name. Return 0 on success, -1 on failure
// Format : send port-number hostname  message
ssize_t bn_send(char** tokens){
    if (tokens[1] == NULL) {
        display_error("ERROR: No port provided", "");
        return -1;
    }

    if (tokens[2] == NULL) {
        display_error("ERROR: No hostname provided", "");
        return -1;
    }

    if (tokens[3] == NULL) {
        display_error("ERROR: No message provided", "");
        return -1;
    }

    int port = atoi(tokens[1]);
    if (port < 1024 || port > 65535) {
        display_error("ERROR: Invalid port number", "");
        return -1;
    }

    char *host = tokens[2];

    int sockfd = connect_to_server(port, host);

    if(sockfd == -1){
        display_error("ERROR: Failed to connect server.", "");
        return -1; 
    }

    int left = MAX_USER_MSG;

    char message[MAX_USER_MSG + 2] = "";
    for (int i = 3; tokens[i] != NULL; i++) {
        if (i > 3 && left > 0) {
            strncat(message, " ", left); 
            left -= 1;
        }
        if (left > 0) {
            int token_len = strlen(tokens[i]);
            int append_len = (token_len < left) ? token_len : left; 
            strncat(message, tokens[i], append_len);
            left -= append_len; 
        }
    }


    // add \r\n before write to socket
    int len = strlen(message);
    message[len] = '\r';
    message[len + 1] = '\n';

    if (write(sockfd, message, len + 2) == -1) {
        display_error("ERROR: Failed to send message", "");
        close(sockfd);
        return -1;
    }

    close(sockfd);
    return 0;

}

// This function allows client to join the server and chat. Return 0 if client joins successfully, else -1. 
ssize_t bn_start_client(char **tokens){
    
    disconnect_client = 0;

    if (tokens[1] == NULL) {
        display_error("ERROR: No port provided", "");
        return -1;
    }
    if (tokens[2] == NULL) {
        display_error("ERROR: No hostname provided", "");
        return -1;
    }

    int port = atoi(tokens[1]);
    if (port < 1024  || port > 65535) {
        display_error("ERROR: Invalid port number", "");
        return -1;
    }
    // Typing Crtl C returns to shell and does not terminate the program
    struct sigaction old_sa;
    sigaction(SIGINT, NULL, &old_sa);

    struct sigaction new_sa;
    new_sa.sa_handler = client_signal_handler;
    sigemptyset(&new_sa.sa_mask);
    new_sa.sa_flags = 0;
    sigaction(SIGINT, &new_sa, NULL);
    //

    int exit_status = 0;
    char *host = tokens[2];

    struct server_socket s;
    memset(s.buf, 0, BUF_SIZE);
    s.inbuf = 0;

    s.sock_fd = connect_to_server(port, host);

    if(s.sock_fd == -1){
        display_error("ERROR: Failed to connect server.", "");
        return -1; 
    }

    fd_set fdset;
    FD_ZERO(&fdset);
    FD_SET(s.sock_fd, &fdset);
    FD_SET(STDIN_FILENO, &fdset);
    
    char io_buf[MAX_USER_MSG + 2];
    int has_unread_data = 0;   

    while(!disconnect_client){
        
        FD_ZERO(&fdset);
        FD_SET(s.sock_fd, &fdset);
    
        if (!has_unread_data) {
            FD_SET(STDIN_FILENO, &fdset);
            if(select(s.sock_fd + 1, &fdset, NULL, NULL, NULL) < 0) {
                
                // disconnect client if crtl + C
                if (errno == EINTR) {
                    if (disconnect_client) {
                        display_message("\n"); 
                        close(s.sock_fd); 
                        sigaction(SIGINT, &old_sa, NULL);
                        return 0;
                    }
                    continue; 
                }
                display_error("ERROR: Failed to Select", "");
                close(s.sock_fd);
                exit(1);
            }
        } else {
            // if there is no additional input, do not wait and read more from stdin. 
            struct timeval tv = {0, 0};
            if(select(s.sock_fd + 1, &fdset, NULL, NULL, &tv) < 0) {
            display_error("ERROR: Failed to Select", "");
            close(s.sock_fd);
            exit(1);
            }
        }

        if (has_unread_data || FD_ISSET(STDIN_FILENO, &fdset)) {
            
            has_unread_data = 0;
            
            if (fgets(io_buf, MAX_USER_MSG + 2, stdin) == NULL) {
                if (feof(stdin)) {
                    display_message("\n"); 
                    close(s.sock_fd);  
                    sigaction(SIGINT, &old_sa, NULL);
                    return 0;
                display_error("ERROR: fgets Failed", "");
                exit(1);
                }
            }
            
            int msg_len = strlen(io_buf);
            if (msg_len == 0) {
                continue;
            }
            
            if(io_buf[msg_len - 1] == '\n') {
                io_buf[msg_len - 1] = '\r';
                io_buf[msg_len] = '\n';
                msg_len++;
            } else {
                ungetc(io_buf[msg_len - 1], stdin);
                has_unread_data = 1;
                io_buf[msg_len - 1] = '\r';
                io_buf[msg_len] = '\n';
                msg_len += 1;
            }

            if (write_to_socket(s.sock_fd, io_buf, msg_len)) {
                display_error("ERROR: Failed to write to socket\n", "");
                exit(1);
            }
        }
        
        // Read server sent messages 
        if (FD_ISSET(s.sock_fd, &fdset)) {
    
            int result = read_from_socket(s.sock_fd, s.buf, &s.inbuf);
            if (result == -1) {
                perror("read");
                exit(1);
            } else if (result == 1) {
                exit(1);
            }
            
            char *msg;
            while (get_message(&msg, s.buf, &s.inbuf) == 0) {
                char *space = strchr(msg, ' ');
                
                if (space != NULL) {
                    *space = '\0'; 

                    char client_buf[BUF_SIZE];
                    snprintf(client_buf, BUF_SIZE, "%s: %s\n", msg, space + 1);
                    display_message(client_buf);

                } else {
                    display_message(msg);
                }           
                free(msg);
            }
        }
    }
    close(s.sock_fd);

    exit(exit_status);
}


    



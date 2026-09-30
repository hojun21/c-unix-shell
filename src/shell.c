#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <ctype.h>
#include "builtins.h"
#include "variables.h"
#include "io_helpers.h"
#include "commands.h"
#include <signal.h>
#include <sys/wait.h>
#include "server.h"

int got_sigint;
char *expanded_tokens[MAX_STR_LEN * 2] = {NULL};
int expanded_token_count = 0;


void handler(){
    display_message("\n");
    // display_message(buffer)
    return;
}

void exit_handler(){
    cleanup_server();
    exit(0); 
}

// You can remove __attribute__((unused)) once argc and argv are used.
int main(__attribute__((unused)) int argc, 
         __attribute__((unused)) char* argv[]) {
    char *prompt = "shell$ "; // TODO Step 1, Uncomment this.

    char input_buf[MAX_STR_LEN + 1]; // input_buf : get input / return total bytes read 
    input_buf[MAX_STR_LEN] = '\0'; 
    char *token_arr[MAX_STR_LEN] = {NULL};

    // Control + C does not terminate the shell
    struct sigaction newact;
    newact.sa_handler = handler;
    sigemptyset(&newact.sa_mask);
    newact.sa_flags = 0;

    if(sigaction(SIGINT, &newact, NULL) == -1){
        display_error("ERROR: sigaction failed", "");
        return -1;
    }

    struct sigaction newact2;
    newact2.sa_handler = exit_handler;
    sigemptyset(&newact2.sa_mask);
    newact2.sa_flags = 0;

    if(sigaction(SIGTERM, &newact2, NULL) == -1){
        display_error("ERROR: sigaction failed", "");
        return -1;
    }
    
    if(sigaction(SIGHUP, &newact2, NULL) == -1){
        display_error("ERROR: sigaction failed", "");
        return -1;
    }

    initialize_available_indices();

    while (1) {
        // Prompt and input tokenization
        // 1. get input string / returns number of bytes read
        // 2. tokenisze_input / return number of token
        // TODO Step 2:
        // Display the prompt via the display_message function.

        // frees all expanded tokens that are expanded 
        

        display_message(prompt);

        int ret = get_input(input_buf);
        size_t token_count;
        char temp_buffer[MAX_STR_LEN * 2 + 1];

        // replace | to " | "
        if (strchr(input_buf, '|') != NULL){
        
            int j = 0;

            for (int i = 0; input_buf[i] != '\0'; i++) {
                if (input_buf[i] == '|') {
                    temp_buffer[j++] = ' ';
                    temp_buffer[j++] = '|';
                    temp_buffer[j++] = ' ';
                } else {
                    temp_buffer[j++] = input_buf[i];
                }
            }
            temp_buffer[j] = '\0';
            token_count = tokenize_input(temp_buffer, token_arr);
        } else {
            token_count = tokenize_input(input_buf, token_arr);
        }

        for (size_t i = 0; i < token_count; i++) {
            if (strchr(token_arr[i], '$') != NULL) {
                char *expanded = expand_variables(token_arr[i]);

                token_arr[i] = expanded;

                expanded_tokens[expanded_token_count++] = expanded;
            }
        }

        display_done_process();
       
     

        // If there is empty string as an input, $shell should continue 
        if (token_count == 0){
            continue;
        }

        // Clean exit
        // TODO: The next line has a subtle issue.
        if (ret != -1 && (strcmp("exit", token_arr[0]) == 0)) {
            remove_all_var();

            cleanup_server();

            exit(0);
        }


        // implementation for background process
        if (strcmp(token_arr[token_count - 1], "&") == 0){
            token_arr[token_count - 1] = NULL;
            token_count--;

            int pid = fork();

            if(pid < 0){
                display_error("ERROR: Fork Failed", "");
                continue;
            }
            // child process runs the code  
            else if(pid == 0){

                int result = execute_command(token_arr, token_count);
                if(result == -1){
                    display_error("ERROR: Unknown command: ", token_arr[0]);
                    exit(1);
                }
                // successfully executed background command
                exit(0);
            }
            // parent process
            else {
                
                // Make command string
                char command[MAX_STR_LEN + 1] = "";
                int remaining = MAX_STR_LEN;
                for(size_t i = 0; i < token_count && token_arr[i] != NULL && remaining > 0; i++){
                    strncat(command, token_arr[i], remaining);
                    remaining -= strlen(token_arr[i]);
                    if(token_arr[i + 1] != NULL){
                        strncat(command, " ", remaining);
                    }   
                }
                command[MAX_STR_LEN] = '\0'; 
                
                // display message
                char buffer[MAX_STR_LEN + 1]; 

                int index = find_new_spot();
                if(index == -1){
                    display_error("ERROR: Too many background processes", "");
                    continue;
                }

                snprintf(buffer, MAX_STR_LEN, "[%d] %d\n", index + 1, pid);
                display_message(buffer);

                add_bg_process(pid, command, index);
                continue;
            }
        }
        
        // if not background process, execute normally 
        else {
            execute_command(token_arr, token_count);
        } 

    }
    return 0;
}
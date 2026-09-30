#define MAX_PATH_LEN 128
#include "io_helpers.h"
#include "commands.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include "builtins.h"
#include "variables.h"


int job_num = 1;
int process_num = 0;
bg_process bg_processes[MAX_PROCESS];
int available_indices[MAX_PROCESS];

// if builtin function is not defined, execute from /bin or /user/bin
int execute_bin(char** tokens){

    pid_t pid = fork();

    if(pid < 0){
        display_error("ERROR: Fork Failed", "");
        return -1;
    }
    // child process execute not builtin command 
    else if(pid == 0){

        char cmd_path[MAX_PATH_LEN + 1];
        
        snprintf(cmd_path, sizeof(cmd_path), "/bin/%s", tokens[0]);
        execv(cmd_path, tokens);

        snprintf(cmd_path, sizeof(cmd_path), "/usr/bin/%s", tokens[0]);
        execv(cmd_path, tokens);
        
        // both failed (builtin does not exist)
        display_error("ERROR: Unknown command: ", tokens[0]);
        _exit(1);

    } else {
        int status;
        waitpid(pid, &status, 0);

        if(WIFEXITED(status) && WEXITSTATUS(status) == 0){
            return 0;
        } 
        
        return -1; 
    }

}

// execute pipeline. Return 0 on success, -1 on failure
int execute_pipeline(char** tokens, int number_of_pipes, int token_count){

    // if there are n pipes, there are n + 1 commands 

    // stores array of each command seperated by "|"
    char *commands[number_of_pipes + 1][128];
    int command_num = number_of_pipes + 1; // number of commands
    
    int command_index = 0;
    int second_index = 0; 
    
    for(int i = 0; i < token_count; i++){
       
        // store first command at commands[0]
        if(command_index == 0){
            if(strcmp(tokens[i], "|") == 0){
                commands[0][second_index] = NULL;
                command_index++;
                second_index = 0;
                continue;
            } else {
                commands[0][second_index] = tokens[i];
                second_index++;
                continue;
            }
        }

        if(strcmp(tokens[i], "|") == 0){
            if(second_index == 0){
                display_error("ERROR: syntax error near unexpected token `|'", "");
                return -1;
            } else {
                commands[command_index][second_index] = NULL;
                command_index++;
                second_index = 0;
                continue;
            }   
        } else {
            commands[command_index][second_index] = tokens[i];
            second_index++;
        }

    }
    // store NULL for execvp 

    commands[command_index][second_index] = NULL;

    int pipes[number_of_pipes][2];
    for(int i = 0; i < number_of_pipes; i++){
        if(pipe(pipes[i]) == -1){

            for(int j = 0; j < i; j++){
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            return -1; 
        }
        // if failed, close all pipes created before
    }

    // stores pid 
    pid_t pids[command_num];

    for(int i = 0; i < command_num; i++){
        pids[i] = fork();

        if(pids[i] < 0){
            display_error("ERROR: Fork Failed", "");
            return -1;
        }
        
        // child process
        else if(pids[i] == 0){
            setbuf(stdout, NULL);
            // read from previous pipe

            if(i > 0){
                dup2(pipes[i - 1][0], STDIN_FILENO);
            }

            if(i < number_of_pipes){
                dup2(pipes[i][1], STDOUT_FILENO);
            }
            // close all after setting up the pipe
            for (int j = 0; j < number_of_pipes; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            // 
            bn_ptr builtin_fn = check_builtin(commands[i][0]);
            
            // Found a function 
            if (builtin_fn != NULL) {   

                ssize_t err = builtin_fn(commands[i]);
            
                if(err == -1){
                    display_error("ERROR: Builtin failed: ", commands[i][0]);
                    exit(1);
                }
                exit(0);

            } else {

                char cmd_path[MAX_PATH_LEN + 1];

                snprintf(cmd_path, sizeof(cmd_path), "/bin/%s", commands[i][0]);
                execv(cmd_path, commands[i]);
                
                snprintf(cmd_path, sizeof(cmd_path), "/usr/bin/%s", commands[i][0]);
                execv(cmd_path, commands[i]);

                display_error("ERROR: Unknown command: ", commands[i][0]);
                exit(1);
            }
            exit(0);
        }
    }
    for (int i = 0; i < number_of_pipes; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
    int status;
    int success = 1;
    
    for (int i = 0; i < command_num; i++) {

        waitpid(pids[i], &status, 0);

        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            success = 0; 
        }
    }
    
    return success ? 0 : -1;
    
}

int execute_command(char** token_arr, int token_count){
    int number_of_pipes = 0;
        int has_pipe = 0;
        int previous_pipe = 0; 
        // check consecutive pipe 
        for(int i = 0; token_arr[i] != NULL; i++){
            if(strcmp(token_arr[i], "|") == 0){
                
                // detect error if there is consecutive pipe 
                if(previous_pipe == 1){
                    display_error("ERROR: syntax error near unexpected token `|'", "");
                    continue;
                }

                has_pipe = 1;
                number_of_pipes++;
                previous_pipe = 1;

            } else {
                previous_pipe = 0;
            }
        }
        
        if(has_pipe){
            // return error if command starts or ends with pipe 
            if(strcmp(token_arr[0], "|") == 0 || strcmp(token_arr[token_count - 1], "|") == 0){
                display_error("ERROR: syntax error near unexpected token `|'", "");
                return -1;
            }

            int result = execute_pipeline(token_arr, number_of_pipes, token_count);
            if(result == -1){
                display_error("ERROR: Pipeline failed: ", "");
                return -1;
            
            // successfully finished pipeline
            } else {
                return 0;
            }
        }
    
        
        // Command execution
        if (token_count >= 1) {
            // builtin_fn = function  / NULL if function does not exist or invalid function 
            bn_ptr builtin_fn = check_builtin(token_arr[0]);
            
            // Found a function 
            if (builtin_fn != NULL) {   
                ssize_t err = builtin_fn(token_arr);

                if (err == - 1) {
                    display_error("ERROR: Builtin failed: ", token_arr[0]);
                    return -1;
                    }
            }
            // check if there is any variables to be assigned  
            // only 1 variable can be assigned at a time 
            else if(strchr(token_arr[0], '=') != NULL && token_count == 1){
                
                // check total number of varialbes to be assigned. 
                if(is_valid_varname(token_arr[0]) == 1){
                    
                   
                    char* key = find_key(token_arr[0]);
                    char* value = find_value(token_arr[0]);

                    // check if there is expansion in $; 
                    char *check_expansion = strchr(value, '$');

                    // there is expansion in the string 
                    if(check_expansion != NULL){
                        char *expanded_value = expand_variables(value);

                        set_variable(key, expanded_value);
                        free(expanded_value);
                    } else {
                        // if there is no expansion 
                        set_variable(key, value);
                    }
                
                    // free key, value 
                    free(key);
                    free(value);
                    return 0;

            
                 
                } // check if the variable is not valid
                else {
                    char* key = find_key(token_arr[0]);
                    display_error("ERROR: Invalid Variable name : ", key);
                    free(key);
                    return -1;
                }
               
            }
            // execute from /usr/bin
            else {
                int result = execute_bin(token_arr);

                if(result == -1){
                    display_error("ERROR: Unknown command: ", token_arr[0]);
                }
                // successfully executed bash built-in command
                return 0; 
                
            }   
        }
        return 0;
}

// Add new process
int add_bg_process(pid_t pid, char *command, int i){
   
    // initialize struct with given information 
    available_indices[i] = 0;
    bg_processes[i].pid = pid;
    bg_processes[i].job_num = i + 1;
    strncpy(bg_processes[i].command, command, MAX_STR_LEN);
    bg_processes[i].command[MAX_STR_LEN - 1] = '\0';
    bg_processes[i].is_running = 1;
    i++;
    return 0;
}

// display all the process that are done. After that, remove done process from the list
void display_done_process(){

    pid_t pid;
    int status;

    // returns 0 if no process is terminated 



    while ((pid = waitpid(-1, &status, WNOHANG)) > 0){
        // if there is at least 1 process executed

        for(int i = 0; available_indices[i] != 1 && i < MAX_PROCESS; i++){
            if(bg_processes[i].pid == pid && bg_processes[i].is_running == 1){
                
                bg_processes[i].is_running = 0;
                available_indices[i] = 2; // 2 implies it is done and needs to be replaced 

                char buffer[MAX_STR_LEN * 2 + 1];
                snprintf(buffer, MAX_STR_LEN * 2, "[%d]+  Done %s\n", 
                    bg_processes[i].job_num, bg_processes[i].command);
                buffer[MAX_STR_LEN * 2] = '\0';
                display_message(buffer);
 
            } 
        }
    }
    for(int i = 0; available_indices[i] != 1 && i < MAX_PROCESS; i++) {
        if(available_indices[i] == 3) {
            bg_processes[i].is_running = 0;
            available_indices[i] = 2; 

            char buffer[MAX_STR_LEN * 2 + 1];
                snprintf(buffer, MAX_STR_LEN * 2, "[%d]+  Done %s\n", 
                    bg_processes[i].job_num, bg_processes[i].command);
                buffer[MAX_STR_LEN * 2] = '\0';
                display_message(buffer);
        }
    }

    int all_terminated = 1;
    // reset all processes if all processors are done. 
    for(int i = 0; i < MAX_PROCESS && available_indices[i] != 1; i++){
        if(available_indices[i] == 0){
            all_terminated = 0;
            break;
        } else {
            continue;
        }
    }

    if(all_terminated){
        initialize_available_indices();
    }

    return; 
}

// initialize so that all processes are available
void initialize_available_indices(){
    for (int i = 0; i < MAX_PROCESS; i++) {
        available_indices[i] = 1;
    }
}

// find new spot for new process
// return -1 if spot is not available
int find_new_spot(){
    for(int i = 0; i < MAX_PROCESS; i++){
        if(available_indices[i] == 1){
            return i;
        }
    }
    return -1;
}


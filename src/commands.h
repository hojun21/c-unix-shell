#ifndef COMMANDS_H
#define COMMANDS_H
#define MAX_PROCESS 1000

int execute_bin(char** tokens);

int execute_pipeline(char** tokens, int number_of_pipes, int token_count);

int execute_command(char **tokens, int token_count);

typedef struct bg_process {
    pid_t pid;
    int job_num;
    char command[MAX_STR_LEN];
    int is_running; // 1 if running, 0 if finished 
} bg_process;

int add_bg_process(pid_t pid, char *command, int index); 

extern bg_process bg_processes[MAX_PROCESS];

extern int process_num; // number of process exists in total

void display_done_process();

extern bg_process bg_processes[MAX_PROCESS];
extern int available_indices[MAX_PROCESS]; // store 1 if empty, 0 if running, 2 if terminated, 3 if killed 
void initialize_available_indices();
int find_new_spot();

#endif // COMMANDS_H;
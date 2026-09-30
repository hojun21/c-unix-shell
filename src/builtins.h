#ifndef __BUILTINS_H__
#define __BUILTINS_H__

#include <unistd.h>
#include "io_helpers.h"


/* Type for builtin handling functions
 * Input: Array of tokens
 * Return: >=0 on success and -1 on error
 */
typedef ssize_t (*bn_ptr)(char **);
ssize_t bn_echo(char **tokens);
ssize_t bn_ls(char **tokens);
ssize_t bn_cd(char **tokens);
ssize_t bn_cat(char **tokens);
ssize_t bn_wc(char **tokens);
ssize_t bn_ps();
ssize_t bn_kill(char **tokens);
ssize_t bn_start_server(char **tokens);
ssize_t bn_close_server(char **tokens);
ssize_t bn_send(char** tokens);
ssize_t bn_start_client(char **tokens);


/* Return: index of builtin or -1 if cmd doesn't match a builtin
 */
bn_ptr check_builtin(const char *cmd);


/* BUILTINS and BUILTINS_FN are parallel arrays of length BUILTINS_COUNT
 */
static const char * const BUILTINS[] = {"echo", "ls", "cd", "cat", "wc", "ps", "kill","start-server","close-server","send", "start-client"};
static const bn_ptr BUILTINS_FN[] = {bn_echo, bn_ls, bn_cd, bn_cat, bn_wc, bn_ps, bn_kill, bn_start_server, bn_close_server, bn_send, bn_start_client, NULL};    // Extra null element for 'non-builtin'
static const ssize_t BUILTINS_COUNT = sizeof(BUILTINS) / sizeof(char *);

// milestone 3
typedef struct {
    char path[MAX_STR_LEN + 1]; // path 
    int has_path;
    int has_rec; // 1 if there is --ref option else 0
    int has_substring; // 1 there is --f option else 0
    int depth; // stores depth, -1 if there is no --d 
    int has_depth; // 1 if there is --d option else 0
    char *substring; // stores substring 
    int valid; // 1 if valid, 0 if invalid
} ls_info;

ls_info parse_ls(char **tokens);

void recursive_ls(char path[], ls_info *info, int depth);


#endif

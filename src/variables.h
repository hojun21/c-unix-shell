#ifndef VARIABLES_H
#define VARIABLES_H

// checks if string s has valid name 
int is_valid_varname(char *s);

// extract varaible name of string a=b
char* find_key(char *s);

// extract value of string a=b
char* find_value(char *s);

// node has key, value and uses linked list attribute 
typedef struct node {
    char *key;
    char *value;
    struct node *next;
} node;


extern node *head;

// stores variable in linked list 
int set_variable(char* key, char* value);

// remove all variables in linked list 
int remove_all_var();

// return value of linked list coressponding to the key. 
char* find_val_with_key(char* key);

// expand variables 
char* expand_variables(char* input);




#endif 
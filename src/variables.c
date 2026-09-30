#include <ctype.h>
#include "variables.h"
#include <stdio.h>
#include "io_helpers.h"
#include <string.h>
#include <stdlib.h>
#include "shell.h"
node *head = NULL;
/*
returns 1 if s has valid name else 0
*/
int is_valid_varname(char *s){  
    // TODO IMPORTANT!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1
    // s must start with a-zA-Z || '_' and it should not contain any special characters

    char* key = find_key(s);

    int length = strlen(key);

    if(!key || !key[0]){
        free(key);
        return 0;
    }
    if(!isalpha(key[0]) && key[0] != '_'){
        free(key);
        return 0; 
    }
    
    for(int i = 1;  i < length; i++){
        // return 0 if there is special character
        if(isalpha(key[i]) || isdigit(key[i]) || key[i] == '_'){
            continue;   
        }
        else {
            free(key);
            return 0; 
        }
    }
    free(key);
    return 1; 
}


// return key of string 
char* find_key(char* s){

    if(s == NULL){
        return NULL;
    }

    //  pos == location of pointer 
    char* pos = strchr(s, '=');

    if(pos == NULL){
        return s;
    }

    // found = 
    int key_len = pos - s;

    char* key = malloc(sizeof(char) * key_len + 1);

    if (!key){
        return NULL;
    }
    strncpy(key, s, key_len);
    key[key_len] = '\0';

    return key;
}

// return value of string 
char* find_value(char*s){
    if(s == NULL){
        return NULL;
    }

    char* pos = strchr(s, '=');

    if(!pos){
        return NULL;
    }

    char* value = strdup(pos + 1);

    return value; 
}

/*
with given value of key, value, store variable in linked list. 
return -1 if stored unsuccessfully,
return  0 if stored successfully. 
*/
int set_variable(char* key, char* value){
    if(!key || !value){
        return -1;
    }

    node *current = head;

    // If there is duplicate, update new value and return 
    while(current){

        ssize_t len_key = strlen(key);
        // if key already exists, dynamically allocate
        if(strncmp(current->key, key, len_key) == 0){
            
            char *new_value = strdup(value);
            if(!new_value) return -1;
            
            // free existing value.
            free(current->value);
            current->value = new_value;
            return 0;
        }
        current = current->next;
    }

    // Since there is no duplicate, we need to create new node 
    node *new_node = malloc(sizeof(node));
    if(!new_node) return -1;

    // store key
    new_node->key = strdup(key);
    // store value 
    new_node->value = strdup(value);
   
    // if either of them is not successfully, stored, then just delete them 
    if(new_node->key == NULL || new_node->value == NULL){
        free(new_node->key);
        free(new_node->value);
        free(new_node);
        return -1;
    }
    new_node->next = head;
    head = new_node;

    return 0; 
}

// return 1 if successfully removed all variable, 0 if not. 
int remove_all_var(){
    node *current = head;

    while(current != NULL){
        node* temp = current->next;
        free(current->key);
        free(current->value);
        free(current);

        current = temp;
    }
    head = NULL;

    for (int i = 0; i < expanded_token_count; i++) {
        if (expanded_tokens[i] != NULL) {
            free(expanded_tokens[i]);
            expanded_tokens[i] = NULL;
        }
    }
    expanded_token_count = 0;

    return 1; 
}

// return value if found
char* find_val_with_key(char* key){
    node *current = head;

    while(current != NULL ){
   
        // if length is different, no need to compare more
        if(strlen(current->key) != strlen(key)){
            current = current->next;
            continue;
        }

        // if same string, return key 
        if(strncmp(key, current->key, strlen(key)) == 0){
            return current->value;
        }

        current = current->next;
    }
    // if key does not exist in the linked list 
    static char empty_string[] = "";
    return empty_string; 

}

// return expanded variables, with given input 
char* expand_variables(char *input){
  
    static char empty_string[] = "";
    if(input == NULL){
        return empty_string;
    }
    

    char* array = malloc(sizeof(char) * (MAX_STR_LEN + 1));
    if(!array){
        display_error("Failed to allocate memory for : ", input);
    }

    size_t remaining = MAX_STR_LEN; 
    memset(array, '\0', MAX_STR_LEN + 1);
    // when input is taking varaible, set to 1, if not, 0.
    int taking_var = 0;
    char var_name[MAX_STR_LEN];
    memset(var_name, '\0', MAX_STR_LEN);
   
    for(size_t i = 0; i < strlen(input); i++){

        if(remaining < 1){
            return array;
        }
            

        if(input[i] == '$'){

            // starting with $ and nothing else to print, just modify taking_var = true; 
            // covers echo $abc.....$
            if(taking_var == 0 && strlen(var_name) == 0){
                taking_var = 1;
                continue;
            }
            
            // print string that is not variable 
            // covers echo HelloWorld$a 
            else if(taking_var == 0 && strlen(var_name) != 0){
                
                var_name[strlen(var_name)] = '\0';
                strncat(array, var_name, remaining);
                remaining -= strlen(var_name);
                taking_var = 1;
    
                memset(var_name, '\0', MAX_STR_LEN);
                continue;
            }
            // $a$a
            else if (taking_var == 1){
                
                var_name[strlen(var_name)] = '\0';
                char *value = find_val_with_key(var_name);

                if(strlen(value) == 0){
                    memset(var_name, '\0', MAX_STR_LEN);
                    continue;
                }

                strncat(array, value, remaining);
                remaining -= strlen(value);
                taking_var = 1;
                memset(var_name, '\0', MAX_STR_LEN);
                continue;
            }

        } 

        // deals with if input is not $ and is regular string. 
        else {
            var_name[strlen(var_name)] = input[i];
            var_name[strlen(var_name)] = '\0';
        }

    }
     
    // After for loop, check if there is values left to be appended 
    if(strlen(var_name) != 0){
            
            if(taking_var == 0){
                strncat(array, var_name, remaining);
                remaining -= strlen(var_name);
                taking_var = 0;
                memset(var_name, '\0', MAX_STR_LEN);
            }
            else {
                char *value = find_val_with_key(var_name);
                strncat(array, value, remaining);
                remaining -= strlen(value);
                taking_var = 0;
                memset(var_name, '\0', MAX_STR_LEN);
            }

        }

    // $ single char 
    if(strlen(var_name) == 0 && taking_var == 1){
        strncat(array, "$", remaining);
        remaining -= 1;
        taking_var = 0;
    } 

    array[MAX_STR_LEN] = '\0';
    return array;

}

        
        
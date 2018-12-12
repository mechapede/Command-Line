//* Utilities for memory management and parsing. All frees are callers responsability */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <ctype.h>

#include "utils.h"

/* Malloc wrapper to track allocations. Aborts program on failure */
void * xmalloc(size_t size) {
    assert(size);
    void * tmp = malloc(size);
    if(!tmp) { //unrecoverable error, if this fails, something is very wrong
        perror("FATAL: A malloc failed: "); // Note this behavior is acceptable for a stand alone program
        abort();                            // but would be insufficient for a library
    }
    return tmp;
}

/* Realloc function to track reallocations. Aborts program on failure */
void * xrealloc(void * ptr, size_t size) {
    assert(size);
    void * tmp = realloc(ptr,size);
    if(!tmp) { //unrecoverable error, if this fails, something is very wrong
        perror("FATAL: A realloc failed: "); // Note this behavior is acceptable for a stand alone program
        abort();                             // but would be insufficient for a library 
    }
    return tmp;
}

/* Free wrapper to track freeing memory */
void xfree(void * ptr) {
    assert(ptr);
    free(ptr);
}

/* Takes a string and attempts to convert it to a valid pid
 * Assumes the string is not empty
 * Return: pid value returned if valid, otherwise -1
 * */
int extract_pid(char * pid){
        assert(pid);
        assert(*pid);
        char * endptr = NULL; //convert token to int
        pid_t pid_num = (pid_t) strtol(pid,&endptr,10);
        if(*endptr) return -1;
        return (int) pid_num;
}


/* Specialized memory freeing function for tokens from get_tokens()*/
void free_tokens(char ** tokens) {
    if(!tokens) return;
    char * * buff = tokens;
    while(*buff) {
        xfree(*buff);
        buff++;
    }
    xfree(tokens);
}

/* Takes an input line and formats it into tokens
 * Returns an array of null terminated strings when tokens
 * Return null if there are no tokens
 * Caller is expected to use free_tokens to cleanup
 */
char * * get_tokens(char * line) {
    int tokens_size = 10;
    char ** tokens = xmalloc(sizeof(char *) * tokens_size);

    int token_index = 0; //where to place next token
    while( *line) { //break into tokens

        while( *line && isspace(*line) ) line++;
        if( !(*line) ) break;

        if (token_index == tokens_size - 1) { //room for null
            tokens_size *= 2;
            tokens = xrealloc(tokens, sizeof(char *) * tokens_size);
        }

        int token_size = 10;
        char * token = xmalloc(sizeof(char*) * token_size);
        int letter_index = 0;
        while( *line && !isspace(*line)) {
            if( letter_index == token_size - 1 ) {
                token_size *= 2;
                char * tmp = realloc(token, sizeof(char) * token_size);
                assert(tmp);
                token = tmp;
            }
            token[letter_index] = *line;
            letter_index++;
            line++;
        }

        token[letter_index] = 0;
        token = xrealloc(token, (letter_index + 1 ) * sizeof(char)); //reduce unused memory

        tokens[token_index] = token;
        token_index++;
    }

    if( token_index == 0) {
        xfree(tokens);
        return NULL;
    } else {
        tokens[token_index] = NULL;
        tokens = realloc(tokens, (token_index + 1)*sizeof(char*)); //save space
        return tokens;
    }
}

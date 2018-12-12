/* Utilities for memory management and parsing. All frees are callers responsability */

#ifndef _UTILS_H
#define _UTILS_H

#include <stdio.h>

/* Malloc wrapper to track allocations. Aborts program on failure */
void * xmalloc(size_t size);

/* Realloc function to track reallocations.  Aborts program on failure */
void * xrealloc(void * ptr, size_t size);

/* Free wrapper to track freeing memory */
void xfree(void * ptr);

/* Takes a string and attempts to convert it to a valid pid
 * Assumes the string is not empty, else behavior may be undefined
 * Return: pid value returned if valid, otherwise -1
 * */
int extract_pid(char * pid);

/* Specialized memory freeing function for tokens from get_tokens()*/
void free_tokens(char ** tokens);

/* Takes an input line and formats it into tokens
 * Returns an array of null terminated strings when tokens
 * Return null if there are no tokens
 * Caller is expected to use free_tokens for cleanup
 */
char * * get_tokens(char * line);

#endif

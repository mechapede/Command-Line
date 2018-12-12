/* Main body of code for the pman */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <assert.h>
#include <sys/types.h>
#include <signal.h>
#include <regex.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "ADTlinkedlist.h"
#include "utils.h"


/* Struct for background programs */
typedef struct subprogram {
    char * name;
    pid_t pid;
} subprogram;

/* Comparison function for subprograms, only compares pid */
int compare_programs(void * val1, void * val2) {
    subprogram * p1 = (subprogram *) val1;
    subprogram * p2 = (subprogram *) val2;
    if ( p1->pid == p2->pid ) {
        return 1;
    }

    return 0;
}

/* Specialized memory freeing funtion ADTLinked node with a subprogram in it*/
void free_node(ADTlinkednode * node) {
    if( node->val) {
        subprogram * prog = (subprogram *) node->val;
        xfree(prog->name);
        xfree(node->val);
    }
    xfree(node);
}

/*
 * Summary: Attempts to create a new process
 * Description: Creates a process using the self-pipe trick to detect
 * failures in exec. Prints an error message on failure.
 * Takes:
 *       programs: linked list of all subprograms
 *       args: array of arguements, 0 assumed to be program name
 */
void create_process(ADTlinkedlist * programs, char * args[]) {

    int pipes[2] = {0}; //assume 0 pipes are unintialized

    if (pipe(pipes) < 0) {
        perror("Aborting. Creating self pipes failed");
        goto end;
    }

    int flags = fcntl(pipes[1], F_GETFD); //set write pipe to close on exec
    if (flags < 0) {
        perror("Aborting. Getting flgas of pipe failed");
        goto end;
    }

    if (fcntl(pipes[1], F_SETFD, flags | FD_CLOEXEC) < 0) {
        perror("Aborting. Could not set flags for pipe");
        goto end;
    }

    pid_t child = fork();
    if(child < 0) {
        perror("Aborting. Creating the child process failed");
    } else if( child == 0 ) { //child action

        if(close(pipes[0]) < 0 ) perror("Warning: Closing read pipe failed");

        execvp(args[0], args); //this will auto-close pipe if it dosen't return

        perror("Aborting. Execv failed (is the program valid?)");
        char fail = 'f'; //if something failed let parrent process know
        write(pipes[1],&fail,sizeof(char));
        if( close(pipes[1]) < 0 ) perror("Closing write pipe failed");
        exit(1);

    } else { //parent action

        if(close(pipes[1]) < 0 ) perror("Warning. Closing write pipe failed"); //close write pipe
        pipes[1] = 0; //zero pipe id

        char code = 0;
        int bytes_read = read(pipes[0],&code,sizeof(char)); //this will return on exec or exec failure
        if(bytes_read < 0) perror("Warning. Reading from pipe failed");

        if( bytes_read) { //child failed to exec since it printed to pipe

            int status = 0;
            if( waitpid(child,&status, 0) < 0 ) perror("Waitpid for exec process failed"); //check status so OS can clean up zombie
            child = -1;

        } else {//othersie the pipe had no output, so exec must have succeded

            subprogram * val = xmalloc(sizeof(subprogram));
            val->pid = child;

            char * name = xmalloc(sizeof(char) * (strlen(args[0])+1) );
            strcpy(name,args[0]);
            val->name = name;

            ADTlinkednode * node = xmalloc(sizeof(ADTlinkednode));
            adtInitiateLinkedNode(node,val);
            adtAddLinkedNode(programs,node,0);

            printf("%s(pid=%d) started\n",args[0],child);
        }

    }

end:
    if(pipes[0] && close(pipes[0]) < 0) perror("Closing read pipe failed"); //close any pipes that are open (non-zero value)
    if(pipes[1] && close(pipes[1]) < 0) perror("Closing write read pipe failed");

}


/* Summary: Send a signal to a process if it is still alive
 * Description: Takes an array of strings of pids that is null terminated
 * Sends the signal to each valid process token
 * Takes:
 *        programs: linked list of all programs
 *        processes: strings of process ids
 *        signal: signal type to send to each process
 */
void send_signal(ADTlinkedlist * programs, char * * processes, int signal) {
    for(; *processes ; processes++) {

        pid_t pid;
        if( (pid = extract_pid(*processes)) == -1 ) {
            printf("Invalid pid, skipping %s\n", *processes);
            continue;
        }

        subprogram comparison; //comparison val, compares function ignores name field
        comparison.pid = pid;
        int index = 0;
        if( (index = adtFindLinkedValue(programs,&comparison,compare_programs)) < 0 ) {
            printf("Cannot send %s to %d(PID UNKNOWN) \n", strsignal(signal), pid);
            continue;
        }

        int status = 0;
        int pid_ret = waitpid(pid,&status,WNOHANG);

        if(pid_ret < 0) {
            perror("Aborting all: a waitpid call failed");
            return;
        }

        if(pid_ret > 0) {
            ADTlinkednode * node = adtPopLinkedNode(programs,index);

            if(WIFSIGNALED(status)) { //two casses
                printf("No signal sent to %s (pid=%d), it has been killed\n",  ((subprogram *) node->val)->name, pid);
            } else if (WIFEXITED(status)) { //two casses
                printf("No signal sent to %s (pid=%d), it has exited\n",  ((subprogram *) node->val)->name, pid);
            } else {
                fprintf(stderr,"WARNING: got signal from waitpid with no handaler, will assume it died!\n");
            }

            free_node(node);

        } else {

            if (kill(pid,signal) == -1) {
                perror("Aborting all. Sending signal failed");
                return;
            }

            printf("%s sent to %d\n",strsignal(signal), pid);

        }

    }
}


 /* Summary: Prints stats for proceses
 * Description: Takes an array of strings of pids that is null terminated
 * Gets the stats for each valid process token
 * Takes:
 *        programs: linked list of all programs
 *        processes: strings of process ids
 */
void print_stats(ADTlinkedlist * programs, char * * processes) {
    int buffer_size = 2000; //io and file name splicing
    char * buffer = xmalloc(sizeof(char)*buffer_size);

    regex_t expr = {0};
    char * pattern = "voluntary_ctxt_switches:[[:space:]{1,}]([[:digit:]]{1,})[[:space:]{1,}]nonvoluntary_ctxt_switches:[[:space:]{1,}]([[:digit:]]{1,})";
    if( regcomp(&expr, pattern, REG_EXTENDED) != 0  ) {
        printf("Aborting all, compiling regex failed\n");
        goto end;
    }

    for(; *processes ; processes++) {

        pid_t pid = 0;
        if( (pid = extract_pid(*processes)) == -1) {
            printf("Skiping invalid program id: %s\n", *processes);
            continue;
        }

        int index = 0;
        subprogram comparison; //comparison val, compare function ignore name field
        comparison.pid = pid;
        if( (index = adtFindLinkedValue(programs,&comparison,compare_programs)) < 0 ) {
            printf("Cannot send signal to pid=%d(UNKNOWN PID)\n",pid);
            continue;
        }

        ADTlinkednode * node = adtPeakLinkedNode(programs, index);

        // to be taken from /proc/pid/stat
        char status = 0;
        double utime = 0;
        double stime = 0;
        long int rss = 0;
        //to be taken from /proc/pid/status
        int voluntary_ctxt_switches = 0;
        int nonvoluntary_ctxt_switches = 0;


        //Each file must be read all at once, since it dosen't support file positions
        snprintf(buffer, buffer_size,"/proc/%d/stat", pid);
        FILE * fp_stat = fopen(buffer,"r");
        if( !fp_stat ) {
            perror("Opening pid file failed");
            printf("Skipping pid = %d .Failed to read from /proc/%d/stat",pid,pid);
            continue; //skip this pid
        }

        int buffer_end = 0; //handle bigger file size
        do {
            buffer_end = fread(buffer,sizeof(char),buffer_size -1, fp_stat);
            if( buffer_end == buffer_size -1) {
                buffer_size *= 2;
                char * tmp = realloc(buffer, buffer_size);
                assert(tmp);
                buffer = tmp;

            }
        } while( buffer_end == buffer_size -1 );
        buffer[buffer_end] = 0;

        fclose(fp_stat);


        if( sscanf(buffer,"%*d %*s %c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lf %lf %*d %*d %*d %*d %*d %*d %*d %*u %ld",&status,&utime,&stime,&rss) != 4 ) {
            printf("Skipping pid = %d .Failed to read from /proc/%d/stat",pid,pid);
            continue;
        } //ugly but nessasary to read /proc/pid/stat

        stime /= sysconf(_SC_CLK_TCK);
        utime /= sysconf(_SC_CLK_TCK);


        snprintf(buffer, buffer_size,"/proc/%d/status", pid);
        FILE * fp_status = fopen(buffer,"r");

        do { //handle posibly bigger file size
            buffer_end = fread(buffer,sizeof(char),buffer_size -1, fp_status);
            if( buffer_end == buffer_size -1) {
                buffer_size *= 2;
                char * tmp = realloc(buffer, buffer_size);
                assert(tmp);
                buffer = tmp;

            }
        } while( buffer_end == buffer_size -1 );

        fclose(fp_status);

        int max_matches = 3;
        regmatch_t matches[max_matches];

        if( regexec(&expr, buffer, max_matches, matches,0) ) {  //check for no matches
            printf("Skipping pid = %d .Failed to find content in /proc/%d/status\n",pid,pid);
            continue;
        }

        int num_chars = matches[1].rm_eo - matches[1].rm_so; //extract voluntary_ctxt_switches
        char * voluntary_ctxt_switches_group = xmalloc(sizeof(char) * (num_chars + 1)); //room for null
        int i;
        for(i =0; i<num_chars; i++) voluntary_ctxt_switches_group[i] = buffer[matches[1].rm_so + i];
        voluntary_ctxt_switches_group[num_chars] = 0;

        char * endptr = 0;
        int tmp = strtol(voluntary_ctxt_switches_group, &endptr, 10);

        if( *endptr == 0 ) {
            voluntary_ctxt_switches = tmp;
            xfree(voluntary_ctxt_switches_group);
        } else {
            printf("Skipping pid = %d .Failed to convert content in /proc/%d/status",pid,pid);
            xfree(voluntary_ctxt_switches_group);
            continue;
        }

        num_chars = matches[2].rm_eo - matches[2].rm_so; //extract non_voluntary_ctxt switches
        char * nonvoluntary_ctxt_switches_group = xmalloc(sizeof(char) * (num_chars + 1)); //room for null
        for(i =0; i<num_chars; i++) nonvoluntary_ctxt_switches_group[i] = buffer[matches[2].rm_so + i];
        nonvoluntary_ctxt_switches_group[num_chars] = 0;

        tmp = strtol(nonvoluntary_ctxt_switches_group, &endptr, 10);

        if(*endptr == 0) {
            nonvoluntary_ctxt_switches = tmp;
            xfree(nonvoluntary_ctxt_switches_group);
        } else {
            printf("Skipping pid = %d .Failed to read from /proc/%d/stat",pid,pid);
            xfree(nonvoluntary_ctxt_switches_group);
            continue;
        }

        printf("Name: %s\n"
               "Pid: %d\n"
               "State: %c\n"
               "Utime: %lf\n"
               "Stime: %lf\n"
               "Rss: %lu\n"
               "Voluntary_ctxt_switches: %d\n"
               "Nonvoluntary_ctxt_swtitches: %d\n\n",
               ((subprogram *) node->val)->name,
               (int) ( (subprogram *) node->val)->pid,
               status,
               utime,
               stime,
               rss,
               voluntary_ctxt_switches,
               nonvoluntary_ctxt_switches);
    }

end:
    regfree(&expr);
    xfree(buffer);

}


/* Summary: Prints all programs that are running or have exited
 * Description: Prints two lists, ended and active programs
 * Takes:
 *        programs: linked list of all programs
 */
void check_execution(ADTlinkedlist * programs) {

    pid_t pid = 1;
    printf("Exited jobs\n"
           "Pid   Name   Exit Reason\n");

    int exited = 0;

    while(pid > 0 && programs->num ) {

        int status = 0;
        pid = waitpid(-1,&status,WNOHANG); // 0 on no child1
        if(pid < 0) {
            perror("Aborintg: A waitpid call failed");
            break;
        }

        if(pid > 0) { //exited child

            subprogram comparison; //comparison val, compare function ignore name field
            comparison.pid = pid;
            int index = adtFindLinkedValue(programs,&comparison,compare_programs);

            if(index < 0) {
                fprintf(stderr,"WARNING: A waitpid call returned an unknown pid\n");
                continue;
            }

            if(index >= 0) { //program exited

                ADTlinkednode * node = adtPopLinkedNode(programs,index); //get node to print info
                exited++;

                if(WIFSIGNALED(status)) { //two casses
                    printf("%d  %s  Killed\n", pid, ((subprogram *) node->val)->name);
                } else if (WIFEXITED(status)) { //two casses
                    printf("%d  %s  Exited\n", pid, ((subprogram *) node->val)->name);
                } else {
                    fprintf(stderr,"WARNING: got signal with no handaler(pid=%d)\n",pid);
                }
                free_node(node);

            }
        }

    }

    printf("Newly finished jobs: %d\n\n",exited);

    printf("Background Jobs\n"
           "Pid    Name\n");

    ADTlinkednode * node = programs->head;
    while(node) {
        subprogram * program = (subprogram *) node->val;
        printf("%d  %s\n", program->pid, program->name);
        node = node->next;
    }

    printf("Total background jobs: %d\n", programs->num);
}



/* Summary: Mainloop for program input and command procesing 
 * Description: Implements commands from assignment
 */
int main() {

    ADTlinkedlist programs; //linked list for subprograms
    adtInitiateLinkedList(&programs); 

    while(1) {
        char * input = NULL;
        input = readline("PMan:  > ");

        if ( input) {
            char * * tokens =  get_tokens(input);
            if(tokens) {
                if(strcmp(tokens[0], "bg") == 0) {
                    if( tokens[1] == NULL) {
                        printf("Program not provided\nusage: bg program [arg1 arg2...]\n");
                    } else {
                        create_process(&programs, tokens + 1);
                    }
                } else if(strcmp(tokens[0],"bglist") == 0) {
                    if( tokens[1] == NULL) {
                        check_execution(&programs);
                    } else {
                        printf("Additional values provided to bglist, should be none\nusage: bglist");
                    }

                } else if(strcmp(tokens[0],"bgkill") == 0) { //ERROR: Process 1245 does not exist.
                    if( tokens[1] == NULL) {
                        printf("No pid provided\nusage: bgkill pid1 [pid2...]\n");
                    } else {
                        send_signal(&programs,tokens+1, SIGKILL);
                    }
                } else if(strcmp(tokens[0],"bgstop") == 0) {
                    if( tokens[1] == NULL) {
                        printf("No pid provided\nusage: bgstop pid1 [pid2...]\n");
                    } else {
                        send_signal(&programs,tokens+1, SIGSTOP);
                    }
                } else if(strcmp(tokens[0],"bgstart") == 0) {
                    if( tokens[1] == NULL) {
                        printf("No pid provided\nusage: bgstart pid1 [pid2...]\n");
                    } else {
                        send_signal(&programs,tokens+1, SIGCONT);
                    }
                } else if(strcmp(tokens[0],"pstat") == 0) {

                    if( tokens[1] == NULL) {
                        printf("No program id provided\nusage: pstat pid1 [pid2...]\n");
                    } else {
                        print_stats(&programs,tokens+1);
                    }
                } else if(strcmp(tokens[0],"help") == 0) {
                    printf("Function            Command:\n"
                           "Start New Program - bg program [arg1 arg2...]\n"
                           "List Program      - bglist\n"
                           "Stats for Program - pstat pid1 [pid2...]\n"
                           "Kill Program      - bgkill pid1 [pid2...]\n"
                           "Stop Program      - bgstop pid1 [pid2...]\n"
                           "Resume Progam     - bgstart pid1 [pid2...]\n");
                } else if(strcmp(tokens[0],"exit") == 0) {
                    free_tokens(tokens);
                    xfree(input);
                    break;

                } else {
                    printf("Unknown command: %s\n", input);
                }
                free_tokens(tokens);
            }
            xfree(input);
        }
    }


    printf("Exiting pman. All background proceses will be left in current state.\n");
    while(programs.num > 0) { //cleanup all nodes
        ADTlinkednode * node = adtPopLinkedNode(&programs,0);
        free_node(node);
    }

    return 0;
}

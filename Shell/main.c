// standard includes
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>

// definitions
#define FINISHED 1
#define SHELL_RL_BUFSIZE 2000
#define SHELL_TOK_BUFSIZE 64
#define SHELL_TOK_DELIM " "

// forward declarations
void shell_loop();
char* shell_read_line(int* status);
void handle_signal();
char** shell_split_line(char* line);

// main
int main(int argc, char **argv) {

    shell_loop();

    return 0;
}

// allocation check
void allocation_check(void* p) {
    if (!p) {
        fprintf(stderr, "Shell: allocation error\n");
        exit(1);
    }
}

// signal handler
void handle_signal(int signum) {
    if (signum == SIGINT) {
        printf("GOT SIGINT\n");
        exit(0);
    }
}

// primary functionality -- shell loop
void shell_loop() {
    // local vars for parsing
    char* line;
    char** args;

    // we'll go until we find eof.
    int status = 0;

    // register handler
    signal(SIGINT, handle_signal);

    // loop forever
    while (1){
        printf("> ");
        line = shell_read_line(&status);
        if (status == FINISHED)
            break;
        printf("%s\n", line);
        args = shell_split_line(line);
        //status = shell_execute(args);
        free(line);
        free(args);

    }
}

char* shell_read_line(int* status) {
    int bufsize = SHELL_RL_BUFSIZE;
    int position = 0;
    char* buf = (char*)malloc(sizeof(char) * bufsize);
    int c;
    allocation_check(buf); 

    while (1) {
        c = getchar();

        if (c == EOF) {
            // break the main loop
            *status = FINISHED;
            buf[position] = 0;
            printf("status is %d\n", *status);
            return buf;
        } else if (c == '\n') {
            // finish the command
            buf[position] = 0;
            return buf;
        }
        else {
            buf[position++] = c;
        }
    }
}

char** shell_split_line(char* line){
    int bufsize = SHELL_TOK_BUFSIZE;
    int position = 0;
    char** tokens = (char**)malloc(bufsize * sizeof(char*));
    char* token;
    allocation_check(tokens);

    // get first token
    token = strtok(line, SHELL_TOK_DELIM);
    while (token != NULL) {
        printf("Token: %s\n", token);
        tokens[position++] = token;

        // too many tokens?
        if (position >= bufsize) {
            bufsize += SHELL_TOK_BUFSIZE;
            tokens = (char**)realloc(tokens, bufsize * sizeof(char*));
            allocation_check(tokens);
        }
        // get next token
        token = strtok(NULL, SHELL_TOK_DELIM);
    }
}

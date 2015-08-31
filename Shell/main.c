// standard includes
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

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
int shell_cd(char** args);
int shell_help(char** args);
int shell_exit(char** args);
int shell_num_builtins();
int shell_execute(char** args);

// constants
const char* BUILTIN_STR[] = {
    "cd",
    "help",
    "exit"
};

int (*BUILTIN_FUNC[]) (char **) = {
    &shell_cd,
    &shell_help,
    &shell_exit
};

// main
int main(int argc, char **argv) {

    shell_loop();

    return 0;
}

int shell_num_builtins() {
    return sizeof(BUILTIN_STR) / sizeof(char*);
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
    while (status != FINISHED){
        printf("> ");
        line = shell_read_line(&status);
        args = shell_split_line(line);
        status |= shell_execute(args);
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
    return tokens;
}

int shell_launch(char** args){
    pid_t pid, wpid;
    int status;
    pid = fork();
    if (pid == 0) {
        if (execvp(args[0], args) == -1) {
            perror("shell");
        }
        exit(1);
    } else if (pid < 0) {
        // for error
        perror("shell");

    } else {
        do {
            wpid = waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
    return 0;
}

int shell_execute(char** args){
    int i;
    if (args[0] == NULL) {
        return 0;
    }
    for (i = 0; i < shell_num_builtins(); ++i){
        if (strcmp(args[0], BUILTIN_STR[i]) == 0) {
            return (*BUILTIN_FUNC[i])(args);
        }
    }
    return shell_launch(args);
}

int shell_cd(char** args){
    if (args[1] == NULL) {
        fprintf(stderr, "Shell: expected argument to \"cd\"\n");
    } else {
        if (chdir(args[1]) != 0) {
            perror("shell");
        }
    }
    return 0;
}

int shell_help(char** args){
    return 0;
}

int shell_exit(char** args){
    return 1;
}

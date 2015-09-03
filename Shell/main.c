/* standard includes*/
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stddef.h>

/* definitions */
#define FINISHED 1
#define SHELL_RL_BUFSIZE 2000
#define SHELL_TOK_BUFSIZE 64
#define SHELL_PIPELINE_BUFSIZE 8
#define SHELL_TOK_DELIM " "
#define INVALID_DESCRIPTOR -1

/* forward declarations */
void shell_loop();
char* shell_read_line(int* status);
void handle_signal();
char** shell_split_line(char* line);
char*** shell_create_pipeline(char** args);
int shell_cd(char** args);
int shell_execute_pipeline(char***);
int shell_help(char** args);
int shell_exit(char** args);
int shell_num_builtins();
int shell_execute(char** args);


/* constants */
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

const char* SPECIAL_CHARS[] = {
    "<",
    ">",
    "2>",
    "2>&1",
    "|",
    "&"
};

/* main */
int main(int argc, char **argv) {

    shell_loop();

    return 0;
}

int shell_num_builtins() {
    return sizeof(BUILTIN_STR) / sizeof(char*);
}

/* allocation check */
void allocation_check(void* p) {
    if (!p) {
        fprintf(stderr, "Shell: allocation error\n");
        exit(1);
    }
}

/* signal handler */
void handle_signal(int signum) {
    if (signum == SIGINT) {
        printf("GOT SIGINT\n");
        exit(0);
    }
}

/* primary functionality -- shell loop */
void shell_loop() {
    /* local vars for parsing */
    char* line;
    char** args;
    char*** pipeline;

    /* we'll go until we find eof. */
    int status = 0;

    /* register handler */
    signal(SIGINT, handle_signal);

    /* loop forever */
    while (status != FINISHED){
        printf("$ ");
        line = shell_read_line(&status);
        args = shell_split_line(line);
        pipeline = shell_create_pipeline(args);
        status |= shell_execute_pipeline(pipeline);
        /*status |= shell_execute(args);*/
        free(line);
        free(args);
        free(pipeline);

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
            *status = FINISHED;
            buf[position] = 0;
            return buf;
        } else if (c == '\n') {
            buf[position] = 0;
            return buf;
        }
        else {
            buf[position++] = c;
        }
    }
}

char** shell_split_line(char* line){
    char* token;
    int bufsize = SHELL_TOK_BUFSIZE;
    int position = 0;
    char** tokens = (char**)malloc(bufsize * sizeof(char*));
    memset(tokens, 0, bufsize);
    allocation_check(tokens);

    token = strtok(line, SHELL_TOK_DELIM);
    while (token != NULL) {
        tokens[position++] = token;

        if (position >= bufsize) {
            bufsize += SHELL_TOK_BUFSIZE;
            tokens = (char**)realloc(tokens, bufsize * sizeof(char*));
            allocation_check(tokens);
        }
        token = strtok(NULL, SHELL_TOK_DELIM);
    }
    return tokens;
}

char*** shell_create_pipeline(char** args){
    int i = 0;
    int j = 0;
    int bufsize = SHELL_PIPELINE_BUFSIZE;
    char*** pipeline = (char***)malloc(bufsize * sizeof(char**));
    memset(pipeline, 0, bufsize);
    allocation_check(pipeline);
    while (args[i] != NULL) {
        pipeline[j++] = &args[i];
        while (args[i] != NULL && strcmp(args[i], "|") != 0){
            ++i;
        }
        if (args[i])
            args[i++] = 0;
    }
    pipeline[j] = NULL;
    i = 0;
    while (pipeline[i] != NULL){
        printf("%s\n", pipeline[i++][0]);
    }
    return pipeline;
}

int shell_execute_pipeline(char*** pipeline){
    int i, child_pid;
    int num_procs = 1;
    int* fds = NULL;
    int pid, wpid, status;
    if (pipeline[0] == NULL){
        return 0;
    }

    while (pipeline[num_procs] != NULL){
        ++num_procs;
    }
    printf("num_procs: %d\n", num_procs);
    if (num_procs > 1) {
        fds = (int*)malloc(2 * (num_procs - 1) * sizeof(int));

        for (i = 0; i < num_procs - 1; ++i){
            pipe(fds + i * 2);
            printf("InputFD: %d\n", *(fds + i * 2));
            printf("OutputFD: %d\n", *(fds + i * 2 + 1));
        }
    }

    for (i = 0; i < num_procs; ++i){
        if (i > 0){
            /* piped input */
            printf("Child %s piping input %d...\n", pipeline[i][0], fds[(i - 1) * 2]);
            if (dup2(fds[(i - 1) * 2], 0) == -1) {
                perror("shell");
                exit(1);
            }
        }
        if (pipeline[i + 1] != NULL) {
            printf("Child %s piping output %d...\n", pipeline[i][0], fds[i * 2 + 1]);
            /* piped output */
            if (dup2(fds[i * 2 + 1], 1) == -1) {
                printf("omg\n");
                perror("shell");
                exit(1);
            }
            printf("got here...");
        }
        printf("calling fork...\n");
        pid = fork();
        if (pid == 0) {
            /* child */
            printf("Child %s executing...\n", pipeline[i][0]);
            if (fds != NULL){
            }
            /* TODO - take a slice of the args */
            if (execvp(pipeline[0][0], pipeline[0]) == -1) {
                perror("shell");
                exit(1);
            } else if (pid < 0) {
                perror("shell");
                exit(1);
            }
        } 

    }

    /* parent code */
    do {
        /* wait for very last process in pipeline */
        wpid = waitpid(pid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));

    /* clean up file descriptors */
    if (fds){
        for (i = 0; i < (num_procs - 1) * 2; ++i){
            printf("Closing %d\n", fds[i]);
            close(fds[i]);
        }
        free(fds);
    }
    return 0;
        

}

int shell_launch(char** args){
    int pid, wpid;
    int status;
    pid = fork();
    if (pid == 0) {
        /* child */
        if (execvp(args[0], args) == -1) {
            perror("shell");
            exit(1);
        }
        exit(1);
    } else if (pid < 0) {
        perror("shell");
        exit(1);

    } else {
        do {
            wpid = waitpid(pid, &status, WUNTRACED);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));
    }
    return 0;
}

int shell_execute(char** args){
    int i = 0;
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
            exit(1);
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

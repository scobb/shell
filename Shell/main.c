/* standard includes*/
#include <malloc.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stddef.h>

/* definitions */
#define FINISHED 1
#define SHELL_RL_BUFSIZE 2000
#define SHELL_TOK_BUFSIZE 64
#define SHELL_PIPELINE_BUFSIZE 8
#define SHELL_TOK_DELIM " "
#define INVALID_DESCRIPTOR -1
#define FILE_SPECIAL_CHARS 4

typedef struct Process{
    char* proc;
    char** args;
} Process;

/* forward declarations */
void shell_loop();
char* shell_read_line(int* status);
void handle_signal();
char** shell_split_line(char* line);
Process* shell_create_pipeline(char** args);
int shell_cd(char** args);
int shell_execute_pipeline(Process*);
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

const char* SPECIAL_CHARS[FILE_SPECIAL_CHARS] = {
    "<",
    ">",
    "2>",
    "2>&1",
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
        _exit(1);
    }
}

/* signal handler */
void handle_signal(int signum) {
    if (signum == SIGINT) {
        printf("GOT SIGINT\n");
        _exit(0);
    }
}

/* primary functionality -- shell loop */
void shell_loop() {
    /* local vars for parsing */
    int i = 0;
    char* line;
    char** args;
    Process* pipeline;

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
        while (pipeline[i].args != NULL){
            /*printf("Freeing args for proc %d at %X: %s\n", i, (int)pipeline[i].args, pipeline[i].proc);*/
            free(pipeline[i++].args);
        }
        /*printf("Freeing pipeline at %X\n", (int)pipeline);*/
        free(pipeline);
        /*printf("Freeing line at %X\n", (int)line);*/
        free(line);
        /*printf("Freeing args at %X\n", (int)args);*/
        free(args);

    }
}

char* shell_read_line(int* status) {
    int bufsize = SHELL_RL_BUFSIZE;
    int position = 0;
    char* buf = (char*)malloc(sizeof(char) * bufsize);
    /*printf("Allocating line to %X\n", (int)buf);*/
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
    /*printf("Allocating args to %X\n", (int)tokens);*/
    allocation_check(tokens);
    memset(tokens, 0, bufsize * sizeof(char*));

    token = strtok(line, SHELL_TOK_DELIM);
    while (token != NULL) {
        tokens[position++] = token;

        if (position >= bufsize) {
            bufsize += SHELL_TOK_BUFSIZE;
            tokens = (char**)realloc(tokens, bufsize * sizeof(char*));
            /*printf("Reallocating args to %X...\n", tokens);*/
            allocation_check(tokens);
            memset(tokens, 0, bufsize * sizeof(char*));
        }
        token = strtok(NULL, SHELL_TOK_DELIM);
    }
    return tokens;
}

Process* shell_create_pipeline(char** args){
    int i = 0;
    int j = 0;
    int k = 0;
    int offset;
    int bufsize = SHELL_PIPELINE_BUFSIZE;
    Process* pipeline = (Process*)malloc(bufsize * sizeof(Process));
    /*printf("allocating pipeline at %X\n", pipeline);*/
    allocation_check(pipeline);
    memset(pipeline, 0, bufsize * sizeof(Process));
    while (args[i] != NULL) {
        pipeline[j].proc = args[i];

        /*pipeline[j++] = &args[i];*/
        while (args[i] != NULL && strcmp(args[i], "|") != 0){
            ++i;
        }
        /*printf("pipline[%d].args: %X\n", j, (int)pipeline[j].args);*/
        pipeline[j].args = (char**) malloc ((i - k + 1) * sizeof(char*));
        allocation_check(pipeline[j].args);
        memset(pipeline[j].args, 0, (i - k + 1) * sizeof(char*));
        offset = k;
        /*printf("%d args allocated for process %d at %X...\n", (i-k+1), j, (int)pipeline[j].args);*/
        for (; k < i; ++k){
            pipeline[j].args[k - offset] = args[k];
            printf("pipeline[%d].args[%d] = %s\n", j, k-offset, args[k]);
        }
        /*printf("Writing NULL to pipeline[%d].args[%d] at %X\n", j, k- offset, (int)pipeline[j].args[k]);*/
        pipeline[j].args[k - offset] = NULL;

        if (args[i]) {
            /*printf("Writing NULL to %X\n", (int)args[i]);*/
            args[i++] = NULL;
        }
        ++j;
        ++k;
    }
    pipeline[j].proc = NULL;
    i = 0;
    while (pipeline[i].proc != NULL){
        printf("%s\n", pipeline[i++].proc);
    }
    return pipeline;
}

int shell_execute_pipeline(Process* pipeline){
    int i, j, k;
    int num_procs = 1;
    int* fds;
    int pid, wpid, status, fd;
    if (pipeline[0].proc == NULL){
        return 0;
    }

    while (pipeline[num_procs].proc != NULL){
        ++num_procs;
    }
    printf("num_procs: %d\n", num_procs);
    if (num_procs > 1) {
        fds = (int*)malloc(2 * (num_procs - 1) * sizeof(int));
        for (i = 0; i < num_procs - 1; ++i) {
            pipe(fds + i * 2);
            printf("InputFD: %d\n", *(fds + i * 2));
            printf("OutputFD: %d\n", *(fds + i * 2 + 1));
        }
    }
    for (i = 0; i < num_procs; ++i){
        printf("calling fork...\n");
        pid = fork();
        printf("pid is %d\n", pid);
        if (pid == 0) {
            /* child */
            /* piped input */
            printf("Child %d: %s\n", i, pipeline[i].proc);
            if (i > 0){
                printf("Child %s piping input fds[%d]: %d...\n", pipeline[i].proc, (i-1)*2, fds[(i - 1) * 2]);
                if (dup2(fds[(i - 1) * 2], 0) == -1) {
                    perror("shell");
                    _exit(1);
                }
                printf("Closing output fds[%d]: %d\n", (i - 1) * 2 + 1, fds[(i-1)*2+1]);
                fflush(stdout);
                close(fds[(i - 1) * 2 + 1]);
            } 
            /* piped output */
            if (pipeline[i + 1].proc != NULL) {
                printf("Closing input fds[%d]: %d\n", i*2, fds[i * 2]);
                fflush(stdout);
                close(fds[i * 2]);
                printf("Child %s piping output fds[%d]: %d...\n", pipeline[i].proc, i*2, fds[i * 2 + 1]);
                fflush(stdout);
                if (dup2(fds[i * 2 + 1], 1) == -1) {
                    printf("omg\n");
                    perror("shell");
                    _exit(1);
                }
            } 
            /* file IO -- supercede the piped output */
            j = 0;
            while (pipeline[i].args[j] != NULL){
                printf("Checking %s for >\n", pipeline[i].args[j]);
                if (strcmp(pipeline[i].args[j], ">") == 0){
                    printf("FOUND IT\n");
                    if (pipeline[i].args[j + 1] != NULL) {
                        fd = open(pipeline[i].args[j+1], O_WRONLY|O_CREAT|O_TRUNC, 0777);
                        if (fd > 0){
                            fchmod(fd, 0644);
                        }
                        printf("Created redirect fd %d for file %s\n", fd, pipeline[i].args[j+1]);
                        if (dup2(fd, 1) == -1) {
                            perror("shell");
                            _exit(1);
                        }
                    }
                    /* stop args here */
                    pipeline[i].args[j] = NULL;
                    ++j;
                    continue;
                }

                printf("Checking %s for <\n", pipeline[i].args[j]);
                if (strcmp(pipeline[i].args[j], "<") == 0){
                    printf("FOUND IT\n");
                    if (pipeline[i].args[j + 1] != NULL) {
                        fd = open(pipeline[i].args[j+1], O_RDWR);
                        /*if (fd < 0){
                            sprintf(buf, "Unable to open %s: no such file\n", pipeline[i].args[j+1]);
                            write(2, buf, strlen(buf));
                        }*/
                        printf("Created redirect fd %d for file %s\n", fd, pipeline[i].args[j+1]);
                        if (dup2(fd, 0) == -1) {
                            perror("shell");
                            _exit(1);
                        }
                    }
                    pipeline[i].args[j] = NULL;
                    ++j;
                    continue;
                }
                
                /*for (k=0; k < FILE_SPECIAL_CHARS; ++k){
                    if (strcmp(SPECIAL_CHARS[k], pipeline[i].args[j]) == 0){

                    }
                }*/
                ++j;
            }


            /* execute */
            printf("executing %s\n", pipeline[i].proc);
            execvp(pipeline[i].proc, pipeline[i].args);
            if (execvp(pipeline[i].proc, pipeline[i].args) == -1) {
                perror("shell");
                _exit(1);
            } else if (pid < 0) {
                perror("shell");
                _exit(1);
            }
            _exit(0);
        } 

    }

    /* parent code */
    do {
        /* wait for very last process in pipeline */
        /*printf("Waiting... on pid %d\n", pid);
        wpid = waitpid(pid, &status, WUNTRACED);*/
        wait(NULL);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));

    /* clean up file descriptors */
    printf("File descriptor cleanup...\n");
    for (i = 0; i < (num_procs - 1) * 2; ++i){
        if (close(fds[i]) != 0){
            printf("error closing fds[%d]\n", i);
        }
    }
/*    if (num_procs > 1){
        close(fds[0]);
        close(fds[1]);
    }*/
    if (fds){
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
            _exit(1);
        }
        _exit(1);
    } else if (pid < 0) {
        perror("shell");
        _exit(1);

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
            _exit(1);
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

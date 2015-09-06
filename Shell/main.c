/* standard includes*/
#include <malloc.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stddef.h>
#include <errno.h>

/* definitions */
#define FINISHED 1
#define SHELL_RL_BUFSIZE 2000
#define SHELL_TOK_BUFSIZE 64
#define SHELL_PIPELINE_BUFSIZE 8
#define SHELL_TOK_DELIM " "
#define INVALID_DESCRIPTOR -1
#define TRUE 1
#define FALSE 0
#define INVALID_JOB_ID 0
#define MAX_JOBS 1000

typedef struct Process{
    char* proc;
    char** args;
    int job_id;
} Process;

/* forward declarations */
void shell_loop();
char* shell_read_line(int* status);
void handle_signal();
char** shell_split_line(char* line, char* bg);
Process* shell_create_pipeline(char** args, int);
int shell_cd(char** args);
int shell_execute_pipeline(Process*, char, int);
int shell_help(char** args);
int shell_exit(char** args);
int shell_num_builtins();
int shell_execute(char** args);


/* constants */
int JOB_ID = 1;
Process PROCESS_TABLE[MAX_JOBS];

const char* BUILTIN_STR[] = {
    "cd",
    "exit"
};

int (*BUILTIN_FUNC[]) (char **) = {
    &shell_cd,
    &shell_exit
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
    int i;
    char* m;
    if (signum == SIGINT) {
        m = "GOT SIGINT\n";
        write(1, m, strlen(m));
        /* _exit(0);*/
        signal(SIGINT, handle_signal);
        siginterrupt(SIGINT, 0);
    }
}
void handle_sigchld(int signum) {
    printf("HANDLER\n");
    int pid = waitpid((pid_t)(-1), 0, WNOHANG);
    while (pid > 0) {
        printf("SIGCHILD pid: %d\n", pid);
        pid = waitpid((pid_t)(-1), 0, WNOHANG);
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
    char bg;

    /* register handler */
    signal(SIGINT, handle_signal);
    siginterrupt(SIGINT, 0);

    struct sigaction sa;
    sa.sa_handler = &handle_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, 0) == -1) {
        perror(0);
        _exit(1);
    }

    /* loop forever */
    while (status != FINISHED){
        bg = FALSE;
        int job_id = JOB_ID++;
        printf("$ ");
        line = shell_read_line(&status);
        args = shell_split_line(line, &bg);
        pipeline = shell_create_pipeline(args, job_id);
        printf("bg: %d\n", bg);
        status |= shell_execute_pipeline(pipeline, bg, job_id);
        /*status |= shell_execute(args);*/
        while (pipeline[i].args != NULL){
            free(pipeline[i++].args);
        }
        free(pipeline);
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

char** shell_split_line(char* line, char* bg){
    char* token;
    int bufsize = SHELL_TOK_BUFSIZE;
    int position = 0;
    char** tokens = (char**)malloc(bufsize * sizeof(char*));
    allocation_check(tokens);
    memset(tokens, 0, bufsize * sizeof(char*));

    token = strtok(line, SHELL_TOK_DELIM);
    while (token != NULL) {
        tokens[position++] = token;

        if (position >= bufsize) {
            bufsize += SHELL_TOK_BUFSIZE;
            tokens = (char**)realloc(tokens, bufsize * sizeof(char*));
            allocation_check(tokens);
            memset(tokens, 0, bufsize * sizeof(char*));
        }
        token = strtok(NULL, SHELL_TOK_DELIM);
    }
    position = 0;
    while (tokens[position]) {
        ++position;
    }
    printf("position: %d\n", position);
    if (position > 0){
        if (strcmp(tokens[position - 1], "&") == 0) {
            *bg = TRUE;
            tokens[position - 1] = NULL;
        }

    }
    return tokens;
}

Process* shell_create_pipeline(char** args, int job_id){
    int i = 0;
    int j = 0;
    int k = 0;
    int offset;
    int bufsize = SHELL_PIPELINE_BUFSIZE;
    Process* pipeline = (Process*)malloc(bufsize * sizeof(Process));
    allocation_check(pipeline);
    memset(pipeline, 0, bufsize * sizeof(Process));
    while (args[i] != NULL) {
        pipeline[j].proc = args[i];
        pipeline[j].job_id = job_id;
        while (args[i] != NULL && strcmp(args[i], "|") != 0){
            ++i;
        }
        pipeline[j].args = (char**) malloc ((i - k + 1) * sizeof(char*));
        allocation_check(pipeline[j].args);
        memset(pipeline[j].args, 0, (i - k + 1) * sizeof(char*));
        offset = k;
        for (; k < i; ++k){
            pipeline[j].args[k - offset] = args[k];
            printf("pipeline[%d].args[%d] = %s\n", j, k-offset, args[k]);
        }
        pipeline[j].args[k - offset] = NULL;

        if (args[i]) {
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

int shell_execute_pipeline(Process* pipeline, char bg, int job_id){
    int i, j, k;
    int num_procs = 1;
    int* fds;
    int pid, wpid, status, fd;
    char* errmsg;
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
    j=0;
    for (i = 0; i < num_procs; ++i){
        printf("calling fork...\n");
        pid = fork();
        printf("pid is %d\n", pid);
        if (pid != 0) {
            /* parent -- insert into process table */
            j = 0;
            while (PROCESS_TABLE[j].job_id != INVALID_JOB_ID) {
                printf("j: %d\n", j);
                ++j;
            }
            printf("Adding table entry for process %s in job %d\n", pipeline[i].proc, pipeline[i].job_id);
            memcpy(&PROCESS_TABLE[j], &pipeline[i], sizeof(Process));
        /*    if (setpgid(pid, job_id+1000) == -1) {
                printf("Error setting group id to %d: %d...\n", job_id + 1000, errno);
            }*/
        } else  {
            /* child */
           /* if (setpgid(getpid(), job_id+1000) == -1) {
                printf("Error setting group id to %d: %d...\n", job_id + 1000, errno);
            }*/
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
                printf("Child %s piping output fds[%d]: %d...\n", pipeline[i].proc, i*2 + 1, fds[i * 2 + 1]);
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
                /*printf("Checking %s for >\n", pipeline[i].args[j]);*/
                if (strcmp(pipeline[i].args[j], ">") == 0){
                    /*printf("FOUND IT\n");*/
                    if (pipeline[i].args[j + 1] != NULL) {
                        fd = open(pipeline[i].args[j+1], O_WRONLY|O_CREAT|O_TRUNC, 0777);
                        if (fd > 0){
                            fchmod(fd, 0644);
                        }
                        /*printf("Created redirect fd %d for file %s\n", fd, pipeline[i].args[j+1]);*/
                        if (dup2(fd, 1) == -1) {
                            perror("shell");
                            _exit(1);
                        }
                    } else {
                        errmsg = "shell: syntax error\n";
                        write(2, errmsg, strlen(errmsg));
                        _exit(1);
                    }
                    /* stop args here */
                    pipeline[i].args[j] = NULL;
                    j += 2;
                    continue;
                }

                /*printf("Checking %s for 2>\n", pipeline[i].args[j]);*/
                if (strcmp(pipeline[i].args[j], "2>") == 0){
                    /*printf("FOUND IT\n");*/
                    if (pipeline[i].args[j + 1] != NULL) {
                        fd = open(pipeline[i].args[j+1], O_WRONLY|O_CREAT|O_TRUNC, 0777);
                        if (fd > 0){
                            fchmod(fd, 0644);
                        }
                        /*printf("Created redirect fd %d for file %s\n", fd, pipeline[i].args[j+1]);*/
                        if (dup2(fd, 2) == -1) {
                            perror("shell");
                            _exit(1);
                        }
                    } else {
                        errmsg = "shell: syntax error\n";
                        write(2, errmsg, strlen(errmsg));
                        _exit(1);
                    }
                    /* stop args here */
                    pipeline[i].args[j] = NULL;
                    j += 2;    
                    continue;
                }

                /*printf("Checking %s for <\n", pipeline[i].args[j]);*/
                if (strcmp(pipeline[i].args[j], "<") == 0){
                    /*printf("FOUND IT\n");*/
                    if (pipeline[i].args[j + 1] != NULL) {
                        fd = open(pipeline[i].args[j+1], O_RDWR);
                        if (fd < 0){
                            errmsg = "shell: no such file\n";
                            write(2, errmsg, strlen(errmsg));
                            _exit(1);
                        }
                        /*printf("Created redirect fd %d for file %s\n", fd, pipeline[i].args[j+1]);*/
                        if (dup2(fd, 0) == -1) {
                            perror("shell");
                            _exit(1);
                        } else {
                            errmsg = "shell: syntax error\n";
                            write(2, errmsg, strlen(errmsg));
                            _exit(1);
                        }
                    }
                    pipeline[i].args[j] = NULL;
                    j+=2;
                    continue;
                }

                /*printf("Checking %s for 2>&1\n", pipeline[i].args[j]);*/
                if (strcmp(pipeline[i].args[j], "2>") == 0){
                    /*printf("FOUND IT\n");*/     
                    if (dup2(1, 2) == -1) {
                        perror("shell");
                        _exit(1);
                    }

                    /* stop args here */
                    pipeline[i].args[j] = NULL;
                    ++j;    
                    continue;
                }

                ++j;
            }


            /* execute */
            printf("executing %s\n", pipeline[i].proc);
            if (strcmp(pipeline[i].proc, "jobs") == 0) {
                printf("PID\tPROCESS\n");  
                for (j = 0; j< MAX_JOBS; ++j){
                    if (PROCESS_TABLE[j].job_id != INVALID_JOB_ID){
                        printf("%d\t%s\n", PROCESS_TABLE[j].job_id, PROCESS_TABLE[j].proc);
                    }
                }
            } else {
                if (execvp(pipeline[i].proc, pipeline[i].args) == -1) {
                    perror("shell");
                    _exit(1);
                } else if (pid < 0) {
                    perror("shell");
                    _exit(1);
                }
            }
            _exit(0);
        } 

    }

    /* parent code */
    printf("File descriptor cleanup...\n");
    for (i = 0; i < (num_procs - 1) * 2; ++i){
        if (close(fds[i]) != 0){
            printf("error closing fds[%d]\n", i);
        }
    }
    /* check for & -- do we wait?*/
    if (!bg) {
        do {
            /* wait for very last process in pipeline */
            /*printf("Waiting... on pid %d\n", pid);
            wpid = waitpid(pid, &status, WUNTRACED);*/
            wait(NULL);
        } while (!WIFEXITED(status) && !WIFSIGNALED(status));    
        /* clean up file descriptors */
        if (fds){
            free(fds);
        }
        /* clean up jobs table */
        for (i = 0; i < MAX_JOBS; ++i){
            if (PROCESS_TABLE[i].job_id == job_id){
                printf("Cleaning up process %d\n", i);
                PROCESS_TABLE[i].job_id = INVALID_JOB_ID;
            }
        }
        
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

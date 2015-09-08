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
#define SHELL_PIPELINE_BUFSIZE 100
#define SHELL_TOK_DELIM " "
#define INVALID_DESCRIPTOR -1
#define TRUE 1
#define FALSE 0
#define INVALID_JOB_ID 0
#define MAX_JOBS 100
#define BACKGROUND 2
#define RUNNING 1
#define STOPPED 0
#define DONE -1
#define KILLED -2

typedef struct Job{
    int job_id;
    char* line;
    struct Job* above;
    struct Job* below;
    int status;
    int pid;
    int* fds;
} Job;

Job* JOB_STACK = NULL;

typedef struct Process{
    /* proc name */
    char* proc;
    /* args */
    char** args;
    /* ids */
    int job_id;
    int pid;
    /* file descriptors */
    int in;
    int out;
    int err;
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
int shell_bg(char**);
int shell_fg(char**);
int shell_jobs(char**);
void create_job_entry(int job_id, char* line);
void remove_job_by_id(int job_id);
void check_jobs();
Job* remove_job_entry(Job*);
/* constants */
int JOB_ID = 1;

const char* BUILTIN_STR[] = {
    "cd",
    "jobs",
    "fg",
    "bg"
};

int (*BUILTIN_FUNC[]) (char **) = {
    &shell_cd,
    &shell_jobs,
    &shell_fg,
    &shell_bg
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
        fprintf(stderr, "yash: allocation error\n");
        _exit(1);
    }
}

/* signal handler */
void handle_signal(int signum) {
    Job* trav = JOB_STACK;
    while (trav) {
        if (trav->status == RUNNING){
            trav->status = KILLED;
            kill(trav->pid, SIGINT);
            break;
        }
        trav = trav->below;
    }
}

void handle_sigchld(int signum) {
    int i;
    Job* j = JOB_STACK;
    int pid = waitpid((pid_t)(-1), 0, WNOHANG);
    while (pid > 0) {
        while (j) {
            if (j->pid == pid) {
                j->status = DONE;
                break;
            }
            j = j->below;
        }
        pid = waitpid((pid_t)(-1), 0, WNOHANG);
    }
}

void handle_sigtstp(int signum) {
    Job* trav = JOB_STACK;
    while (trav) {
        if (trav->status == RUNNING){
            trav->status = KILLED;
            kill(trav->pid, SIGTSTP);
            break;
        }
        trav = trav->below;
    }
}

/* primary functionality -- shell loop */
void shell_loop() {
    /* local vars for parsing */
    int i = 0;
    char* line;
    char** args;
    Process* pipeline;
    struct sigaction sa;

    /* we'll go until we find eof. */
    int status = 0;
    char bg;

    /* register handlers */
    sa.sa_handler = &handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sa, 0) == -1) {
        perror(0);
        _exit(1);
    }

    sa.sa_handler = &handle_sigtstp;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGTSTP, &sa, 0) == -1) {
        perror(0);
        _exit(1);
    }

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
        fflush(stdout);
        printf("$ ");
        line = shell_read_line(&status);
        check_jobs();
        create_job_entry(job_id, line);
        args = shell_split_line(line, &bg);
        pipeline = shell_create_pipeline(args, job_id);
        status |= shell_execute_pipeline(pipeline, bg, job_id);
        i = 0;
        while (pipeline[i].args != NULL){
            free(pipeline[i++].args);
        }
        free(pipeline);
        free(line);
        free(args);

    }
}
void check_jobs() {
    Job* cleanup;
    Job* trav = JOB_STACK;
    char plusminus = '+';
    while (trav) {
        if (trav->status == DONE) {
            printf("[%d] %c Done\t%s\n", trav->job_id, plusminus, trav->line);
            trav = remove_job_entry(trav);
        } else if (trav->status == KILLED) {
            trav = remove_job_entry(trav);
        } else {
            trav = trav->below;
        }
        plusminus = '-';
    }
}
void create_job_entry(int job_id, char* line) {
    Job* j = (Job*)malloc(sizeof(Job));
    j->job_id= job_id;
    j->line = (char*)malloc((strlen(line) + 1) * sizeof(char));
    strcpy(j->line, line);
    j->below = JOB_STACK;
    j->above = NULL;
    j->status = RUNNING;
    j->fds = NULL;
    j->pid = 0;
    if (j->below)
        j->below->above = j;
    JOB_STACK = j;
}
Job* get_job_by_id(int job_id) { 
    Job* trav = JOB_STACK;
    while (trav) {
        if (trav->job_id == job_id) {
            return trav;
        }
    }
    return NULL;
}
void remove_job_by_id(int job_id) {
    Job* trav = JOB_STACK;
    while (trav) {
        if (trav->job_id == job_id) {
            remove_job_entry(trav);
            break;
        }
        trav = trav->below;
    }
}

Job* remove_job_entry(Job* j) {
    if (j->above) {
        if (j->below)
            j->below->above = j->above;
        j->above->below = j->below;

    } else {
        JOB_STACK = j->below;
        if (j->below)
            j->below->above = NULL;
    }
    Job* ret = j->below;
    free(j->line);
    free(j);
    return ret;
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
    /*printf("position: %d\n", position);*/
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
        pipeline[i].in = 0;
        pipeline[i].out = 1;
        pipeline[i].err = 2;
        while (args[i] != NULL && strcmp(args[i], "|") != 0){
            ++i;
        }
        pipeline[j].args = (char**) malloc ((i - k + 1) * sizeof(char*));
        allocation_check(pipeline[j].args);
        memset(pipeline[j].args, 0, (i - k + 1) * sizeof(char*));
        offset = k;
        for (; k < i; ++k){
            pipeline[j].args[k - offset] = args[k];
        }
        pipeline[j].args[k - offset] = NULL;

        if (args[i]) {
            args[i++] = NULL;
        }
        ++j;
        ++k;
    }
    pipeline[j].proc = NULL;
    return pipeline;
}

int shell_execute_pipeline(Process* pipeline, char bg, int job_id){
    int i, j, k, table_ind;
    int num_procs = 1;
    int* fds = NULL;
    int pid, wpid, status, fd, group;
    char* errmsg;
    group = -1;
    if (pipeline[0].proc == NULL){
        remove_job_by_id(job_id);
        return 0;
    }

    while (pipeline[num_procs].proc != NULL){
        ++num_procs;
    }
    /*printf("num_procs: %d\n", num_procs);*/
    if (num_procs > 1) {
        fds = (int*)malloc(2 * (num_procs - 1) * sizeof(int));
        for (i = 0; i < num_procs - 1; ++i) {
            pipe(fds + i * 2);
            /*printf("InputFD: %d\n", *(fds + i * 2));
            printf("OutputFD: %d\n", *(fds + i * 2 + 1));*/
        }
    }
    j=0;
    table_ind = 0;
    for (i = 0; i < num_procs; ++i){
        for (k = 0; k < shell_num_builtins(); ++k) { 
            if (strcmp(BUILTIN_STR[k], pipeline[i].proc) == 0) {
                /*printf("Found built-in %s...\n", BUILTIN_STR[k]);*/
                remove_job_by_id(pipeline[i].job_id);
                return BUILTIN_FUNC[k](pipeline[i].args);
            }
        } 
        /*printf("calling fork...\n");*/
        pid = fork();
        /*printf("pid is %d\n", pid);*/
        if (pid != 0) {
            /* parent -- insert into process table */
            if (group == -1){
                group = pid;
            }
            if (setpgid(pid, group) != 0) {
                perror("yash");
                _exit(1);
            } 
            Job* trav = get_job_by_id(job_id);
            if (!trav->pid) {
                trav->pid = pid;
            }
            pipeline[i].pid = pid;
        } else  {
            /* child */
            /* close any earlier FDs */
            for (j = 0; j < (i-1) * 2; ++j) {
                /*printf("Child %s closing fds[%d]: %d\n", pipeline[i].proc, j, fds[j]);*/
                close(fds[j]);
            }
            for (j = i*2+2; j < (num_procs - 1) * 2; ++j) {
                /*printf("Child %s closing fds[%d]: %d\n", pipeline[i].proc, j, fds[j]);*/
                close(fds[j]);
            }
            /* piped input */
            if (i > 0){
                pipeline[i].in = fds[(i - 1) * 2];
                if (dup2(pipeline[i].in, 0) == -1) {
                    perror("yash");
                    _exit(1);
                }
                close(fds[(i - 1) * 2 + 1]);
            } 
            /* piped output */
            if (pipeline[i + 1].proc != NULL) {
                pipeline[i].out = fds[i * 2 + 1];
                close(fds[i * 2]);
                if (dup2(fds[i * 2 + 1], 1) == -1) {
                    perror("yash");
                    _exit(1);
                }

            } 
            /* file IO -- supercede the piped output */
            j = 0;
            while (pipeline[i].args[j] != NULL){
                if (strcmp(pipeline[i].args[j], ">") == 0){
                    if (pipeline[i].args[j + 1] != NULL) {
                        fd = open(pipeline[i].args[j+1], O_WRONLY|O_CREAT|O_TRUNC, 0777);
                        /* update permissions */
                        if (fd > 0){
                            fchmod(fd, 0644);
                        }
                        if (pipeline[i].err == pipeline[i].out) {
                            pipeline[i].err = fd;
                            if (dup2(fd, STDERR_FILENO) == -1) {
                                perror("yash");
                                _exit(1);
                            }
                        }
                        pipeline[i].out = fd;
                        if (dup2(fd, STDOUT_FILENO) == -1) {
                            perror("yash");
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

                if (strcmp(pipeline[i].args[j], "2>") == 0){
                    if (pipeline[i].args[j + 1] != NULL) {
                        fd = open(pipeline[i].args[j+1], O_WRONLY|O_CREAT|O_TRUNC, 0777);
                        if (fd > 0){
                            fchmod(fd, 0644);
                        }
                        pipeline[i].err = fd;
                        if (dup2(fd, STDERR_FILENO) == -1) {
                            perror("yash");
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

                if (strcmp(pipeline[i].args[j], "<") == 0){
                    if (pipeline[i].args[j + 1] != NULL) {
                        fd = open(pipeline[i].args[j+1], O_RDWR);
                        if (fd < 0){
                            errmsg = "shell: no such file\n";
                            write(2, errmsg, strlen(errmsg));
                            _exit(1);
                        }
                        pipeline[i].in = fd;
                        if (dup2(fd, STDIN_FILENO) == -1) {
                            perror("yash");
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

                if (strcmp(pipeline[i].args[j], "2>&1") == 0){
                    pipeline[i].err = pipeline[i].out;
                    if (dup2(pipeline[i].out, STDERR_FILENO) == -1) {
                        perror("yash");
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
            if (execvp(pipeline[i].proc, pipeline[i].args) == -1) {
                fprintf(stderr, "yash: %s: command not found\n", pipeline[i].proc);
                _exit(1);
            } else if (pid < 0) {
                perror("yash");
                _exit(1);
            }
            _exit(0);
        } 

    }

    /* parent code */
    for (i = 0; i < (num_procs - 1) * 2; ++i){
        if (close(fds[i]) != 0){
            printf("error closing fds[%d]\n", i);
        }
    }
    /* clean up file descriptors */
    if (fds){
        free(fds);
    }
    /* check for & -- do we wait?*/
    if (!bg) {
        do {
            /* wait for last in pipeline to finish */
            wpid = waitpid(pid, &status, WUNTRACED);
            if (WIFSTOPPED(status)) {
                Job* j = get_job_by_id(job_id);
                j->status = STOPPED;
                return 0;
            }
        } while (!WIFEXITED(status) && !WIFSIGNALED(status) && !WIFSIGNALED(status));    
        /* clean up jobs table */
        remove_job_by_id(job_id);
        
    } else {
        Job* j = get_job_by_id(job_id);
        j->status = BACKGROUND;
    }

    return 0;
}

int shell_cd(char** args){
    if (args[1] == NULL) {
        fprintf(stderr, "Shell: expected argument to \"cd\"\n");
    } else {
        if (chdir(args[1]) != 0) {
            perror("yash");
        }
    }
    return 0;
}
int shell_jobs(char** args) {
    int j;
    Job* trav = JOB_STACK;
    char plusminus = '+';
    char* running = "Running";
    char* stopped = "Stopped";
    while (trav) {
        if (trav->status) {
            printf("[%d] %c %s\t%s\n", trav->job_id, plusminus, running, trav->line);
        } else {
            printf("[%d] %c %s\t%s\n", trav->job_id, plusminus, stopped, trav->line);
        }
        trav = trav->below;
        plusminus = '-';
    }
}

int shell_fg(char** args) {
    Job* trav = JOB_STACK;
    int i, wpid, status;
    char plusminus = '+';
    while (trav) {
        if (trav->status == BACKGROUND || trav->status == STOPPED) {
            trav->status = RUNNING;
            kill(trav->pid, SIGCONT);
            printf("%s\n", trav->line);
            fflush(stdout);
            do {
                wpid = waitpid(trav->pid, &status, WUNTRACED);
                if (WIFSTOPPED(status)) {
                    trav->status = STOPPED;
                    return 0;
                }
            } while (!WIFEXITED(status) && !WIFSIGNALED(status));
            remove_job_by_id(trav->job_id);
        }
        plusminus = '-';
        trav = trav->below;
    }

}
int shell_bg(char** args) {
    Job* trav = JOB_STACK;
    int i, wpid, status;
    while (trav) {
        if (trav->status == BACKGROUND || trav->status == STOPPED) {
            trav->status = RUNNING;
            kill(trav->pid, SIGCONT);
            printf("%s\n", trav->line);
            fflush(stdout);
        }
        trav = trav->below;
    }

}

int shell_help(char** args){
    return 0;
}

int shell_exit(char** args){
    return 1;
}

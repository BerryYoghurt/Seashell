#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAXINPUT 100 //max input length

int log_file;
pid_t seq_child_pid = -1;

_Bool parse_input(char*, char**,int*);

void child_terminated(int, siginfo_t*, void*);

void execute_process(char**);

void print_exact_size(char*, int);

int main() {
    char* write_buf;
    log_file = open("TerminateChildren.log",O_WRONLY|O_CREAT|O_APPEND, S_IRUSR | S_IWUSR);
    if(log_file <= 0){
        write_buf = "Error in creating the log file.\n";
        print_exact_size(write_buf, STDOUT_FILENO);
        write(STDOUT_FILENO,write_buf,sizeof(write_buf));
        exit(-1);
    }
    write_buf = "~~~NEW SESSION~~~\n";
    print_exact_size(write_buf, log_file);

    _Bool run = 1, background;
    char input[MAXINPUT];
    char** args = malloc(60*sizeof(char*));//no need to allocate memory for each entry because they already exist in input
    char* pwd = malloc(MAXINPUT*sizeof(char));
    int error_code;

    struct sigaction sa;
    sa.sa_sigaction = child_terminated;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGCHLD,&sa,NULL);

    while(run){
        sprintf(write_buf, "~~%s~~>", getcwd(pwd, MAXINPUT));
        //printf("~~%s/~~>", getcwd(pwd, MAXINPUT));
        print_exact_size(write_buf, STDOUT_FILENO);
        //fgets(input,MAXINPUT,stdin);
        read(STDOUT_FILENO,input,MAXINPUT);
        //printf("input is %s", input);

        background = parse_input(input,args,&error_code);

        /*for(int i = 0; i < 4; i++){
            printf("argument %d is %s\n", i, args[i]);
        }*/
        //printf("error_code is %d\n", error_code);

        if(strcmp(args[0], "exit") == 0){
            run = 0;
        }else if(strcmp(args[0],"cd") == 0){
            chdir(args[1]);
        }else if(background){
            if(fork() == 0){
                execute_process(args);
            }
        }else{
            if((seq_child_pid = fork()) == 0){
                execute_process(args);
            }else{
                //printf("PID %d should be waited on\n", seq_child_pid);
                waitpid(seq_child_pid, &error_code, 0);
                //printf("PID %d should've exited\n",seq_child_pid);
                /*if(WIFEXITED(error_code)){
                    printf("CHILD EXITED\n");
                }*/
            }
        }
    }
    close(log_file);
    free(pwd);
    free(args);
    return 0;
}

/**
 * @param input is the raw line typed through the shell
 * @param args is the command and options after being parsed. Must be an allocated array
 * @param error_code marks errors.. for now if the line doesn't end after 100 characters
 * @return true iff there is an apersand at the end of the input**/
_Bool parse_input(char* input, char** args, int* error_code){
    char** entry = args;
    *error_code = 0;
    int i = 0;
    _Bool amp = 0;
    /*for(int j = 0; j < 20; j++){
        printf("input at %d is %c\n", j,input[j]);
    }*/
    while(i < MAXINPUT && input[i] != '\n' && input[i] != '&'){
        //skip all whitespace
        while(i < MAXINPUT && (input[i] == ' ' || input[i] == '\t')){
            input[i] = '\0';
            i++;
        }
        //have we consumed the input yet?
        if(i >= MAXINPUT || input[i] == '\n' || input[i] == '&'){
            break;
        }
        //if not, register argument
        *entry = &input[i];
        entry++;
        //skip all non-whitespace
        while(i < MAXINPUT &&
                !(input[i] == '&' || input[i] == '\n' || input[i] == ' ' || input[i] == '\t')){
            i++;
        }
    }

    if(i < MAXINPUT){
        if(input[i] == '&'){
            input[i++] = '\0';
            amp = 1;
        }
        if(i < MAXINPUT && input[i] == '\n'){
            input[i] = '\0';
        }
    }else{
        *error_code = 1;
    }


    *entry = NULL; //last entry
    return amp;
}

void child_terminated(int signum, siginfo_t* info, void * ucontext){
    //write(STDOUT_FILENO,"RECEIVED SIGCHLD\n",20);
    if(info->si_pid != seq_child_pid) {//if the sequential child is the one who sent this signal, ignore it
        char buf[50];
        int wstatus;
        pid_t pid = wait(&wstatus);
        sprintf(buf, "Child with PID %d terminated with status %d\n", pid, wstatus);
        print_exact_size(buf,log_file);
        //write(STDOUT_FILENO,"TERMINATED!!\n",50);
    }
}

void execute_process(char** args){
    execvp(args[0],args);
    print_exact_size("No such program was found, are you sure it's installed and in the system path?\n",STDOUT_FILENO);
    exit(-1);
}

/**
 * wrapper around write in order to write the exact number of lines**/
void print_exact_size(char* buf, int fd){
    while(*buf != '\0'){
        write(fd,buf++,1);
    }
}
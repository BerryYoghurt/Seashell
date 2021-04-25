#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAXINPUT 100 //max input length

int log_file;
pid_t seq_child_pid = -1;//sequential child ID

/**
 * @param input is the raw line typed through the shell
 * @param args is the command and options after being parsed. Must be an allocated array
 * @param error_code marks errors.. for now if the line doesn't end after 100 characters
 * @return true iff there is an apersand at the end of the input**/
_Bool parse_input(char*, char**,int*);


/**
 * handler for SIGCHLD signal, logs the PID of the terminated child, its exit status, and whether it was synchronous or asynchronous*/
void child_terminated(int, siginfo_t*, void*);

/**
 * a wrapper around exec call, only to make code cleaner*/
void execute_process(char**);

/**
 * wrapper around write in order to write the exact number of lines.. to provide signal safety**/
void print_exact_size(char*, int);

int main() {
    char* write_buf = malloc(MAXINPUT*sizeof(char));
    log_file = open("TerminatedChildren.log",O_WRONLY|O_CREAT|O_APPEND, S_IRUSR | S_IWUSR);
    if(log_file <= 0){
        write_buf = "Error in creating the log file.\n";
        print_exact_size(write_buf, STDOUT_FILENO);
        exit(-1);
    }
    print_exact_size("~~~NEW SESSION~~~\n", log_file);

    _Bool run = 1, background;
    char input[MAXINPUT];
    char** args = malloc(60*sizeof(char*));//no need to allocate memory for each entry because they already exist in input
    char* pwd = malloc(MAXINPUT*sizeof(char));
    int error_code;

    struct sigaction sa;
    sa.sa_sigaction = child_terminated;
    sa.sa_flags = SA_SIGINFO|SA_RESTART;
    sigaction(SIGCHLD,&sa,NULL);

    while(run){
        sprintf(write_buf, "~~%s~~>", getcwd(pwd, MAXINPUT));
        print_exact_size(write_buf, STDOUT_FILENO);

        read(STDIN_FILENO,input,MAXINPUT);
        background = parse_input(input,args,&error_code);

        if(error_code == 1){
            print_exact_size("Input too long.\n", STDOUT_FILENO);
            continue;
        }else if(error_code == 2){
            print_exact_size("Quotes were not closed or input too long.\n", STDOUT_FILENO);
            continue;
        }

        if(args[0] == NULL){//empty line
            continue;
        }else if(strcmp(args[0], "exit") == 0){
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
                waitpid(seq_child_pid, &error_code, 0);
            }
        }
    }

    close(log_file);
    free(write_buf);
    free(pwd);
    free(args);
    return 0;
}


_Bool parse_input(char* input, char** args, int* error_code){
    char** entry = args;
    *error_code = 0;
    int i = 0;
    _Bool amp = 0;

    while(i < MAXINPUT && input[i] != '\n' && input[i] != '&'){
        //skip all whitespace
        while(i < MAXINPUT && (input[i] == ' ' || input[i] == '\t')){
            input[i] = '\0';
            i++;
        }
        //have we consumed the input yet?
        if(i >= MAXINPUT || input[i] == '\n' || input[i] == '&'||input[i] == '\0'){
            break;
        }
        //if not, register argument


        //if in quote mode, keep everything as it is
        if(input[i] == '"'){//accepts only double quotes
            input[i++]='\0';
            *entry = &input[i];

            while(i < MAXINPUT && input[i++] != '"');

            input[i-1] = '\0';
            if(i >= MAXINPUT){
            //consumed max number of input characters but the quote didn't end
                *error_code = 2;
            }
        }else{
            //else skip all non-whitespace
            *entry = &input[i];
            while(i < MAXINPUT &&
                  !(input[i] == '&' || input[i] == '\n' || input[i] == ' ' || input[i] == '\t')){
                i++;
            }
        }
        entry++;

    }

    if(i < MAXINPUT){
        if(input[i] == '&'){
            input[i++] = '\0';
            amp = 1;
        }
        if(i < MAXINPUT && input[i] == '\n'){
            input[i] = '\0';
        }
    }else if (*error_code == 0){//no other error
        *error_code = 1;
    }


    *entry = NULL; //last entry
    return amp;
}

void child_terminated(int signum, siginfo_t* info, void * ucontext){
    char buf[100];
    int wstatus;
    pid_t pid;
    if(info->si_pid != seq_child_pid) {//if the sequential child is the one who sent this signal, ignore it

        pid = wait(&wstatus);
        sprintf(buf, "Asynchronous child with PID %d terminated with status %d\n", pid, wstatus);
        print_exact_size(buf,log_file);
    }else{
        pid = info->si_pid;
        wstatus = info->si_status;
        sprintf(buf, "Synchronous child with PID %d terminated with status %d\n", pid, wstatus);
        print_exact_size(buf,log_file);
    }
}

void execute_process(char** args){
    execvp(args[0],args);
    print_exact_size("No such program was found, are you sure it's installed and in the system path?\n",STDOUT_FILENO);
    exit(-1);
}


void print_exact_size(char* buf, int fd){
    while(*buf != '\0'){
        write(fd,buf++,1);
    }
}
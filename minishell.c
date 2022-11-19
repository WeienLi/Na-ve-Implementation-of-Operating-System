// DISCLAIMER: This code is given for illustration purposes only. It can contain bugs! // You are given the permission to reuse portions of this code in your assignment.
//
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>

struct jobstruct
{
    pid_t pid;
    char * name;
};
struct jobstruct jobs1[150];
// This code is given for illustration purposes. You need not include or follow this
// strictly. Feel free to write better or bug free code. This example code block does not // worry about deallocating memory. You need to ensure memory is allocated and deallocated // properly so that your shell works without leaking memory.
//


void insert(pid_t id,char*string){
    for (int i = 0;i<149;i++){
        if (jobs1[i].pid == 0){
            jobs1[i].pid = id;
            jobs1[i].name = string;
            break;
        }
    }
}
void remove1(pid_t id){
    for (int i = 0;i<149;i++){
        if (jobs1[i].pid == id){
            jobs1[i].pid = 0;
            jobs1[i].name = NULL;
            break;
        }
    } 
}
void job(){
    int count = 0;
    printf("Here are the list of background jobs\n");
    printf("fg_num \t pid \t CMD \n");
    for (int i = 0;i<149;i++){
        if ((int)jobs1[i].pid > 0 ){
            printf("%d \t %d \t %s\n",count,jobs1[i].pid,jobs1[i].name);
            count ++;
        }
    }
    // printf("Count %d", count);
}

int getcmd(char *prompt, char *args[], int *background){
    int length, i = 0; 
    char *token, *loc; 
    char *line = NULL; 
    size_t linecap = 0;
    printf("%s", prompt);
    length = getline(&line, &linecap, stdin);
    if (length <= 0) {
        exit(-1); 
    }
// Check if background is specified.. 
    if ((loc = index(line, '&')) != NULL) { 
        *background = 1; //don't wait concurrent
        *loc = ' '; 
    } else
        *background = 0;
    while ((token = strsep(&line, " \t\n")) != NULL) {
        for (int j = 0; j < strlen(token); j++)
            if (token[j] <= 32) 
                token[j] = '\0';
        if (strlen(token) > 0) 
            args[i++] = token;
        }
    args[i] = '\0';
    return i; 
}
void sigchld_handler(int signum)
{
    pid_t pid;
    while ((pid = waitpid(-1, NULL, WNOHANG)) != -1)
    {
           remove1(pid);
           return; // Or whatever you need to do with the PID
    }
}
//main above are helpers
int main(void){
    char *args[20];
    int bg;
    for(int i = 0; i < 149; i ++){
        jobs1[i].pid = 0;
    }
    while(1) {
        signal(SIGINT,SIG_IGN);
        signal(SIGTSTP,SIG_IGN);
        signal(SIGCHLD,sigchld_handler);
        bg = 0;
        int cnt = getcmd("\n>> ", args, &bg);
        if (cnt == 0){
            continue;
        }
        char *s = args[0];
        //exit command
        if (strcmp(s,"exit")==0){
            exit(0);
        }
        // cd command
        if(strcmp(args[0],"cd")==0){
            if(args[1] != NULL){
                chdir(args[1]);
            }
            continue;
        }
        //jobs command
        if(strcmp(args[0],"jobs")==0){
            job();
            continue;
        }
        //fg command enter index to bring it to fg
        if (strcmp(args[0],"fg")==0){
            int counter = 0;
            if(args[1] != NULL){
                int x = atoi(args[1]);
                for (int i = 0;i<149;i++){
                    if (x == counter){
                        pid_t id = jobs1[i].pid;
                        jobs1[i].pid = 0;
                        jobs1[i].name = NULL;
                        waitpid(id,NULL,0);
                        break;
                    }
                    else if(jobs1[i].pid != 0){
                        counter ++;
                    }
                } 
            }
            continue;
        }
        //pipeline
        char * args1[20];
        char * args2[20];
        int mark = 0;
        int i, counter,k;
        for(i = 0; i<20;i++){
            if(args[i] == NULL){
                break;
            }
            else if (strcmp(args[i],"|")==0){
                mark ++;
                args1[i] = NULL;
                i ++;
                break;
            }
            else if(mark == 0){
                args1[i] = args[i];
            }
        }
        counter = 0;
        if (mark > 0){
            for (k = i; k<20;k++){
                if(args[k] == NULL){
                break;
                }
                else{
                    args2[counter] = args[k];
                    counter ++;
                }
            }
            args2[counter] = NULL; 
            int pip[2];
            if(pipe(pip) == -1){
                perror("pipelining failed");
                exit(1);
            }
            //first child
            if(fork()==0){
                signal(SIGINT,SIG_DFL);
                close(STDOUT_FILENO);
                dup(pip[1]);
                close(pip[0]);
                close(pip[1]);
                execvp(args1[0],args1);
            }
            //child 2
            if(fork()==0){
                signal(SIGINT,SIG_DFL);
                close(STDIN_FILENO);
                dup(pip[0]);
                close(pip[1]);
                close(pip[0]);
                execvp(args2[0],args2);

            }
            close(pip[0]);
            close(pip[1]);
            wait(0);
            wait(0);
            continue;
        }
        // end of pipeline
        //creating child process 
        pid_t id = fork();
        if (id == 0){
            char *new[20];
            int j;
            char * filename = NULL;
            signal(SIGINT,SIG_DFL);
            //redirection
            for(j=0;j<20;j++){
                if (args[j] != NULL){
                    if(strcmp(args[j],">")==0){
                    filename = args[j+1];
                    break;
                }
                    new[j] = args[j];
            }
                else{
                        break;
                }
            }
            new[j] = NULL;
            if(filename != NULL){
                int f;
                if ((f = open(filename, O_WRONLY | O_CREAT, 0644)) < 0)
                {
                    perror("couldn't open output file.");
                    exit(0);
                }

                // args+=2;
                dup2(f, STDOUT_FILENO);
                close(f);
            }
            int x = execvp(new[0],new);
            remove1(id);
            exit(0);
        }
        else{
            insert(id,args[0]);
            if (bg == 0){ //without &
                waitpid(id,NULL,0);
                remove1(id);
            }
        }
    }
}

/* the steps can be..:
(1) fork a child process using fork()
(2) the child process will invoke execvp()
(3) if background is not specified, the parent will wait...*/
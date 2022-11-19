#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include "helper.h"
#include "sut.h"
#include "queue.h"
#include <sys/types.h> 
#include <sys/stat.h> 

int num_tasks = 0;
int num_exit = 0;
pthread_t idc;
pthread_t idi;
pthread_t idc2;
pthread_mutex_t c_mutex = PTHREAD_MUTEX_INITIALIZER; // cexec mutex
pthread_mutex_t i_mutex = PTHREAD_MUTEX_INITIALIZER; // iexec mutex
ucontext_t memo,c_context,i_context;
ucontext_t memo2,c_context2;
struct queue readyq;
struct queue waitq;
int numcpu = 2; // num  of c-exec (1 for part A 2 for part B)
bool finish = false; //flag to check whether to exit all thread.

void *cexec(){
    while(true){ 
        if(finish){ // a flag to exit all thread.
            pthread_exit(NULL);
        }
        pthread_mutex_lock(&c_mutex);
        struct queue_entry *next= queue_pop_head(&readyq); //use mutex to protect pop head.
        pthread_mutex_unlock(&c_mutex);
        if(next){
            c_context = *(ucontext_t *)next->data;
            //nanosleep((const struct timespec[]){{0, 100000L}}, NULL);
            //free entry.
            swapcontext(&memo, &c_context); //save currenet context and run next's context
        }
        nanosleep((const struct timespec[]){{0, 100000L}}, NULL);
        //free(next);
    }
}

void *cexec2(){ //function for second cpu similar to above.
    while(true){
        if(finish){
            pthread_exit(NULL);
        }
        pthread_mutex_lock(&c_mutex);
        struct queue_entry *next= queue_pop_head(&readyq);
        pthread_mutex_unlock(&c_mutex);
        if(next){
            c_context2 = *(ucontext_t *)next->data;
            swapcontext(&memo2, &c_context2); //run next context
        }
        nanosleep((const struct timespec[]){{0, 100000L}}, NULL);
        //free(next);
    }
}

void *iexec(){
    struct queue_entry *next;
    while(true){
        if(finish){
            pthread_exit(NULL);
        }
        pthread_mutex_lock(&i_mutex);
        next= queue_pop_head(&waitq); //next node in waitq
        pthread_mutex_unlock(&i_mutex);
        if(next){ 
            iohelper *nextio = (iohelper *)next->data;
            int mode = nextio -> mode;
            if(mode == 1){ //check the mode aka read write open close see more in helper.h
                char * filen = nextio -> filename; //load fiield then system call
                int fpointer = open(filen,O_RDWR);
                nextio -> fd = fpointer;
                nextio -> check = true; //make return check true so busy waiting know it io is done and ready to return
                //if there is return.
            }
            else if(mode ==2){
                int size = nextio->sizes;
                int fpoint = nextio ->fd;
                char * buf = nextio->buffer;
                write(fpoint,buf,size);
                nextio -> check = true;
            }
            else if(mode == 4){
                //printf("Starting to read...\n");
                int size = nextio->sizes;
                int fpoint = nextio ->fd;
                char * buf = nextio->buffer;
                read(fpoint,nextio->buffer,size);
                nextio -> check = true;
            }
            else if(mode == 3){
                int fpoint = nextio -> fd;
                close(fpoint);
                nextio -> check = true;
            }
        }
        nanosleep((const struct timespec[]){{0, 100000L}}, NULL);
        //free(next); 
    }
}
void sut_init(){ //intialize all global variables that are needed.
    readyq = queue_create();
    queue_init(&readyq);
    waitq = queue_create();
    queue_init(&waitq);
    getcontext(&memo);
    getcontext(&memo2);
    finish = false ;
    num_exit = 0;
    num_tasks = 0;
    pthread_create(&idc,NULL,cexec,NULL);
    pthread_create(&idi,NULL,iexec,NULL);
    if (numcpu == 2){
        pthread_create(&idc2,NULL,cexec2,NULL);
    }
    
}

bool sut_create(sut_task_f fn) { //add the function to cexec queue.
    bool x;
    struct queue_entry *p;
    threaddesc *tdescrpitor = malloc(sizeof(threaddesc));
    //provided in YAU
    getcontext(&(tdescrpitor->threadcontext)); 
    tdescrpitor->threadid = num_tasks;
    tdescrpitor->threadstack = (char *)malloc(THREAD_STACK_SIZE);
    tdescrpitor->threadcontext.uc_stack.ss_sp = tdescrpitor->threadstack; 
    tdescrpitor->threadcontext.uc_stack.ss_size = THREAD_STACK_SIZE;
    tdescrpitor-> threadcontext.uc_link = 0; 
    tdescrpitor-> threadcontext.uc_stack.ss_flags = 0;
    tdescrpitor->threadfunc = &fn;
    makecontext(&(tdescrpitor->threadcontext), fn, 0);
    //store the context in node
    struct queue_entry *cur = queue_new_node(&(tdescrpitor->threadcontext));
    pthread_mutex_lock(&c_mutex);
    queue_insert_tail(&readyq, cur); //add the new node to tail.
    num_tasks++;
    pthread_mutex_unlock(&c_mutex);
    STAILQ_FOREACH(p, &readyq, entries){ //check whether it is added successfully or not
        x = (p==cur);
    }
    return x;
}

void sut_yield(){
    pthread_t tid ;
    tid = pthread_self();
    ucontext_t current;
    getcontext(&current); //store current context
    //ucontext_t current2;
    //getcontext(&current2);
    struct queue_entry *new = queue_new_node(&current);
    pthread_mutex_lock(&c_mutex); //add it back to queue
    queue_insert_tail(&readyq, new);
    pthread_mutex_unlock(&c_mutex);
    if(pthread_equal(tid,idc)){ //check which cpu it is using
        swapcontext(&current, &memo);
    }
    else{
        swapcontext(&current, &memo2);
    }
}

void sut_exit(){
    pthread_t tid ;
    tid = pthread_self();
    nanosleep((const struct timespec[]){{0, 100000L}}, NULL);
    pthread_mutex_lock(&c_mutex); //get both io and c lock to gaurantee exit success.
    pthread_mutex_lock(&i_mutex);
    if(queue_peek_front(&readyq) == NULL && queue_peek_front(&waitq) == NULL ){
        //printf("Canceling...\n");
        //nanosleep((const struct timespec[]){{0, 1000000000L}}, NULL); 
        finish = true;
        return;
    }
    pthread_mutex_unlock(&c_mutex);
    pthread_mutex_unlock(&i_mutex);
    ucontext_t current;
    getcontext(&current);
    if(pthread_equal(tid,idc)){ //check which cpu it is using
        swapcontext(&current, &memo);
    }
    else{
        swapcontext(&current, &memo2);
    }
}

void sut_shutdown() {
    pthread_join(idc, NULL);
    pthread_join(idi, NULL);
    if(numcpu == 2){
        pthread_join(idc2,NULL);
    }
    pthread_mutex_destroy(&c_mutex);
    pthread_mutex_destroy(&i_mutex);
}

int sut_open(char *fname){ //load parameter in struct
    pthread_t tid = pthread_self();
    iohelper *iodescrpitor = malloc(sizeof(iohelper));
    iodescrpitor->filename = fname;
    iodescrpitor->check = false;
    iodescrpitor->mode =  1;

    struct queue_entry *curr = queue_new_node(iodescrpitor);
    pthread_mutex_lock(&i_mutex);
    queue_insert_tail(&waitq,curr); //add it to the waiting q/
    pthread_mutex_unlock(&i_mutex);

    ucontext_t current;
    getcontext(&current);
    iodescrpitor->threadcontext = current;

    struct queue_entry *cpunew = queue_new_node(&current);
    pthread_mutex_lock(&c_mutex);
    queue_insert_tail(&readyq,cpunew); //add it back to cpu q
    pthread_mutex_unlock(&c_mutex);
    if(pthread_equal(tid,idc)){
        swapcontext(&current, &memo);
    }
    else{
        swapcontext(&current, &memo2);
    }
    while(!iodescrpitor->check);//busy wait
    int result = iodescrpitor -> fd;
    free(iodescrpitor);//free
    return result;
}
void sut_write(int fd, char *buf, int size){//similar logic to above
    pthread_t tid = pthread_self();
    iohelper *iodescrpitor = malloc(sizeof(iohelper));
    iodescrpitor -> sizes = size;
    iodescrpitor -> buffer = buf;
    iodescrpitor ->fd = fd;
    iodescrpitor -> mode = 2; 
    iodescrpitor->check = false;
    struct queue_entry *curr = queue_new_node(iodescrpitor);
    pthread_mutex_lock(&i_mutex);
    queue_insert_tail(&waitq,curr);
    pthread_mutex_unlock(&i_mutex);
    ucontext_t current;
    getcontext(&current);
    iodescrpitor->threadcontext = current;
    struct queue_entry *cpunew = queue_new_node(&current);
    pthread_mutex_lock(&c_mutex);
    queue_insert_tail(&readyq,cpunew);
    pthread_mutex_unlock(&c_mutex);
    if(pthread_equal(tid,idc)){
        swapcontext(&current, &memo);
    }
    else{
        swapcontext(&current, &memo2);
    }
    while(!iodescrpitor->check);//busy wait
    free(iodescrpitor);
}
void sut_close(int fd){//similar logic to above
    pthread_t tid = pthread_self();
    iohelper *iodescrpitor = malloc(sizeof(iohelper));
    iodescrpitor -> fd = fd;
    iodescrpitor -> mode = 3;
    iodescrpitor->check = false;
    struct queue_entry *curr = queue_new_node(iodescrpitor);
    pthread_mutex_lock(&i_mutex);
    queue_insert_tail(&waitq,curr);
    pthread_mutex_unlock(&i_mutex);
    ucontext_t current;
    getcontext(&current);
    iodescrpitor->threadcontext = current;
    struct queue_entry *cpunew = queue_new_node(&current);
    pthread_mutex_lock(&c_mutex);
    queue_insert_tail(&readyq,cpunew);
    pthread_mutex_unlock(&c_mutex);
    if(pthread_equal(tid,idc)){
        swapcontext(&current, &memo);
    }
    else{
        swapcontext(&current, &memo2);
    }
    while(!iodescrpitor->check);//busy wait
    free(iodescrpitor);
}
char *sut_read(int fd, char *buf, int size){//similar logic to above
    pthread_t tid = pthread_self();
    iohelper *iodescrpitor = malloc(sizeof(iohelper));
    iodescrpitor-> fd = fd;
    iodescrpitor-> mode = 4;
    iodescrpitor->buffer = buf;
    iodescrpitor->sizes = size;
    iodescrpitor->check = false;

    struct queue_entry *curr = queue_new_node(iodescrpitor);
    pthread_mutex_lock(&i_mutex);
    queue_insert_tail(&waitq,curr);
    pthread_mutex_unlock(&i_mutex);

    ucontext_t current;
    getcontext(&current);
    iodescrpitor->threadcontext = current;

    struct queue_entry *cpunew = queue_new_node(&current);
    pthread_mutex_lock(&c_mutex);
    queue_insert_tail(&readyq,cpunew);
    pthread_mutex_unlock(&c_mutex);

    if(pthread_equal(tid,idc)){
        swapcontext(&current, &memo);
    }
    else{
        swapcontext(&current, &memo2);
    }
    while(!iodescrpitor->check);//busy wait
    char * result = iodescrpitor->buffer;
    free(iodescrpitor);
    return result;
}
/*
// The main function to test
int main()
{
    sut_init();
    // bool r1 = sut_create(display);
    // sut_yield();
    bool r1 = sut_create(display);
    //bool r2 = sut_create(display2);
    // printf("%d\n", r1);
    // printf("%d\n", r2);
    sut_shutdown();
    return 0;
}
*/


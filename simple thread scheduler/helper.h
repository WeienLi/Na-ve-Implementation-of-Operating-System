#define _XOPEN_SOURCE

#ifndef __YAUTHREAD_H__
#define __YAUTHREAD_H__

#include <ucontext.h>


#define MAX_THREADS                        32
#define THREAD_STACK_SIZE                  1024*64

#define RR                                 1   // round robin
#define FCFS                               2   // first come first served

#define RR_QUANTUM                         2   // in seconds


typedef struct __threaddesc
{
	int threadid;
	char *threadstack;
	void *threadfunc;
	ucontext_t threadcontext;
} threaddesc;

typedef struct __iohelper
{
	int fd;
	int mode; //1 for open 2 for write 3 for close 4 for read
	//void *threadfunc;
	ucontext_t threadcontext;
	char * filename;
	bool check;
	char * buffer;
	int sizes;
} iohelper;



extern threaddesc threadarr[MAX_THREADS];
extern int numthreads, curthread;
extern ucontext_t parent;

#endif

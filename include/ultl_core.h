/* < Edit date: 22/02/14 > */

/*********************************************************
 * This open source verison of ULTL. 
 * Product Date: 01-01-2014.
 * Version: 2.2467.
 * Developed By: Swarvanu Sengupta(NTIL)
 *
 * File Info: File holds the interface and structure
 *********************************************************/

#include <setjmp.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h> 
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <linux/sched.h>
#include "ultl_err.h"

#define ULTL_SIGNAL SIGUSR2
#define ULT_STACK_SIZE_64BIT (1024*1024*16) /* 16 MB STACK */
#define ULT_STACK_SIZE_32BIT (1024*1024*8)  /* 8 MB STACK */
#define ULT_STACK_SIZE_16BIT (1024*128)     /* 128 KB STACK */
#define FAILURE -1
#define SUCCESS 0
#define INVALID -1
#define TRUE 1
#define FALSE 0
#define MAX_PRIORITY 10
#define CLOCK_STACK_SIZE 128*1024
#define D_PRIORITY 5
#define D_RR_TIME 5
#define MAX_INTERRUPT 10
#define D_EXITSTATUS -999
#define D_LJ_RETURN 1
#define ISR_LJ_RETURN 2

#define NOT_LAST(ptr) ptr->next != NULL 

#define ultl_malloc(size) malloc(size)
#define ultl_free(ptr) free(ptr)

#define ultl_info(msg, ...) fprintf(stdout, "INFO: " "%s: " msg "\n", __func__, ##__VA_ARGS__)
#define ultl_debug(msg, ...) fprintf(stdout, "DEBUG: " "%s: " msg "\n", __func__, ##__VA_ARGS__)
#define ultl_error(error, ...) fprintf(stderr, "ERROR: " "%s: " error "\n", __func__, ##__VA_ARGS__)
#define ultl_critical(error, ...) fprintf(stderr, "CRITICAL ERROR: " "%s: " error "\n", __func__, ##__VA_ARGS__)

typedef enum{LOCKED, UNLOCKED}t_lock_status; 
typedef enum{NEW, READY, RUNNING, WAITING, TERMINATE, ABORT, INTERRUPT}t_thread_state;
typedef enum{RR, FCFS, PRIORITY}t_scheduler_type;
typedef enum{IN_1 = 1, IN_2, IN_3, IN_4, IN_5, IN_6, IN_7, IN_8, IN_9, IN_10}t_interrupts; 
typedef enum{SOFT_LOCK = 0, HARD_LOCK}t_locktype;

/* thread structure holds information of a ulthread */  
typedef struct ulthread {
    jmp_buf buf;             /* stores the current thread context */
    stack_t stack;           /* signal stack ( thread stack ) */
    t_thread_state state;    /* current state of the thread */
    t_thread_state t_state;  /* temp state to hold thread pre interrupt state */
    int id;                  /* id of the following thread */ 
    int priority;            /* true priority of the thread */
    int lock_owned;          /* ID of a lock if owned */
    int u_priority;          /* user priority of the thread */
    void (*fptr) (void*);    /* body of the thread */
    void *args;              /* arguments to be passed on thread */ 
    int exit_status;         /* exit_status of the thread */
    struct ulthread *next;   /* points to next ulthread obj */
    struct ulthread *prev;   /* points to prev ulthread onb */
}st_ulthread;

/* ready queue structure (double queue) */
typedef struct readyqueue {
    struct ulthread *start;  /* start of the readyqueue */
    struct ulthread *end;    /* end of the readyqueue */
    int nos;                 /* no of elements in readyqueue */
}st_readyqueue;

/* structure holding thread reference and next node */ 
typedef struct threadreffnode {
    st_ulthread *thread;
    struct threadreffnode * next;
}st_trefnode;

/* structure to hold all the thread ref in a linkedlist manner */ 
typedef struct threadreffmap {
    st_trefnode *array;
    int nos;
}st_trefmap;

/* structure for holding all the scheduler specific ver */
typedef struct scheduler {
    jmp_buf main;                     /* holds main context */
    int pre_interrupt_thread;         /* holds the thread ID if interrupt is 
                                       * generated from a thread */ 
    st_ulthread *current_thread;      /* current thread reference otherwise NULL */ 
    int inthread;                     /* flag determines if in thread */ 
    int ininterrupt;                  /* flag determines if in interrupt handling */ 
    unsigned int type;                /* current type of scheduler */
    int RR_sleep_time;                /* round robin slot interval */
    int clock_pid;                    /* pid of the clock process */
    void *clock_pstack;               /* stack address of the clock process */
    void *RR_sigstack;                /* signal stack address for RR*/
}st_scheduler;

/* structure holds the core variables */
struct ultl_core {
    st_readyqueue ready_queue;  /* ready queue */ 
    st_scheduler scheduler;    /* thread scheduler */
    st_trefmap thread_ref_map; /* thread reference */
};

/* event structure hold event inf and thread waiting for it */
typedef struct event {
    int id;              /* event id */
    st_trefnode *array;  /* array holding reference of waiting threads */
    int nos;             /* nos of thread waiting for event */
    struct event *next;  /* points to the next events */
}st_event;

/* structure to hold all the
 * events ref in a linkedlist manner */
struct eventreffmap {
    st_event *list;      /* list holding reference to events */
    int nos;             /* nos of events in map */
};

/* Structure to hold thread info for a specific interrupt */ 
typedef struct interrrupt_thread_info {
    int thread_id;                       /* thread id */
    int in_service_routine;              /* flag to indicate if ISR is being performed */
    void (*service_routine) (int);       /* service routine body */	
    struct interrrupt_thread_info *next; /* next thread */
}st_interrupt_thread;

/* interrupt Structure to hold all the inf */
typedef struct interrupt {
    int id;                        /* interrupt id */
    st_interrupt_thread *list;     /* array of thread info for interrupt */
    int nos;                       /* no of element in array */
}st_interrupt;  

typedef struct interrupt_stack_node {
    int called_thread_id;              /* called thread id (0 if main)*/
    int tergeted_thread_id;            /* tergeted thread id */
    int interrupt_id;                  /* interrupt id <tentative> */
    struct interrupt_stack_node *next; /* node reference to stack */
}st_interrupt_stack_node;

typedef struct interrupt_stack {
    st_interrupt_stack_node *top;   /* Top of the stack */
    int nos;                        /* Current nos of node in stack */
}st_interrupt_stack;

/* structure to hold all the interrupt ref in a linkedlist manner */ 
struct interruptreffmap {
    st_interrupt array[MAX_INTERRUPT];      /* array holding interrupt 
	                                     * reference */
    st_interrupt_stack pre_interrupt_stack; /* stack holding current 
	                                     * interrupt chain */ 
    int current_interrupt_id;               /* current interrupt id */
};  

/* key Structure to hold all the information */
typedef struct key {
    int thread_id;     /* owner thread id (who locked it) */
    int id;            /* key id */
    int lock_type;     /* type of the lock */
    int lock_status;   /* holds lock status locked or unlocked */
    struct key * next; /* reference to the next key in keymap */
}st_key;

/* structure to hold all the key ref in a linkedlist manner */
struct keymap {
    st_key * array;    /* array holding refference to the keys */
    int nos;           /* nos of keys in key refference map */
}; 

/* Basic thread functions */
int ult_init(void);
int ult_create(void(*fun_ptr)(void*),void *arg);
int ult_start(int *thread_array, int nos);
void ult_yeild(void);
int ult_exit(int exit_status);
int ult_free(int tid);

/* Thread waiting and sleeping functions */
int ult_create_event(void);
int ult_sleep(int sec);
int ult_wait(int event_id);
int ult_notify(int event_id);

/* Thread Interrupt and signalling functions */
int ult_create_interrupt(void (*serviceroutine)(int), int interrupt_id);
int ult_raise_interrupt(int interrupt_id);
int ult_interrupt_thread(int interrupt_id, int thread_id);

/* Thread lock and synchronization functions */
int get_key(int lock_type);
int ult_lock(int key_id);
int ult_unlock(int key_id);

/* Thread utility functions */
int ult_get_tid(void); 
int get_exit_status(int thread_id);
int ult_scheduler(int type);
int set_ult_priority(int thread_id, int priority); 
int get_ult_priority(int thread_id);

/* Debugging function */
int ult_get_state_(int t_id);
int ult_get_stackspace_(int thread_id);
int print_ready_queue_(void);
int print_waiting_threads_(int event_id);

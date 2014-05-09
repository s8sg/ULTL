/*********************************************************
 * This open source verison of ULTL. 
 * Product Date: 01-01-2014.
 * Version: 2.2467.
 * Developed By: NTI
 *
 * File Info: File holds the definetion of ULTL functions
 *            and core logic.
 *********************************************************/ 

#include "ultl_core.h"   /* Header defines all the necessery definations */


/* Defination of internal function used by ULTL */
static st_trefnode *_new_thread_node(void(*fun_ptr)(void*),void *);
static st_ulthread * _get_ref_byid(int);
static int _bring_in_readyq(int);
static st_event* _get_event_ref_byid(int);
static int _readyq_priority_sort(void);

/* Defination of Internal variable used by ULTL */ 
static struct ultl_core g_ultl_core;
static struct eventreffmap g_event_map;
static struct interruptreffmap g_interrupt_map;
static struct keymap g_key_map;
static int ult_init_ver = 0;    

/* _RRscheduler_clock() : process that works as a clock for 
 *                       Round Robin Scheduling.
 * @data :               NULL. For future use.
 */
void _RRscheduler_clock(void* data) {
   int ppid;
   ppid = getppid();
   printf("new p: In new process: %d\n", ppid);
   while(1) {
       printf("new P:(%d) Sending signal to (%d)\n", getpid(), ppid);
       kill(ppid, ULTL_SIGNAL);
       printf("new p:Sleep time: %d\n", g_ultl_core.scheduler.RR_sleep_time);
       sleep(g_ultl_core.scheduler.RR_sleep_time);
   } 
}

static void _RR_schedule_handler(int sig) {

   ultl_info(">> Signal recived: \n");
   if(g_ultl_core.scheduler.inthread == TRUE) {
       ult_yeild();
   }
}
    
/* _thread_scheduler(): function to schedule threads in readyqueue 
 * return:    on success: return 0, 
 *            on failure: returns Error code
 *
 * readyqueue threads are scheduled by this function. it schedules 
 * threads and jumps to the conext of schedule thread 
 */  
static void _thread_scheduler() {  
       
     st_ulthread *ref_current = NULL;     /* current thread reff */           
     /* check if PRIORITY */ 
     //else if(g_ultl_core.scheduler.type == PRIORITY){
          /* short ready queue in basis of priority */
     printf("Before short: ");
     print_ready_queue_();
     _readyq_priority_sort();
     printf("After Short short: ");
     /*XXX*/print_ready_queue_();
     /* get the ref of the thread to be dispatched */
     ref_current = g_ultl_core.ready_queue.start;
     /* set the state as RUNNING */ 
     ref_current->state = RUNNING;
     /* set inthread flag */ 
     g_ultl_core.scheduler.inthread = TRUE; 
     /* set curent thread in scheduler */
     g_ultl_core.scheduler.current_thread = ref_current;		  
     printf("Address of buffer: %x \n", ref_current->buf);
     longjmp(ref_current->buf, D_LJ_RETURN);
     //}    
}

/* ultl_init(): Initialize the core of ULTL 
 * return:    on success: return 0, 
 *            on failure: returns Error code
 *
 * Initialize all the internal structures of thread library
 */ 
int ult_init(void) {
 
    int count;  /* initialize counter */

    if(ult_init_ver != 0) {
       ultl_error("ULTL core is already initialized");
       return -E_ALRDYINIT;
    }

    /* init thread reff map structure */
    ultl_info("\ninit THREAD REFF MAP... \n");
    g_ultl_core.thread_ref_map.array = NULL;
    g_ultl_core.thread_ref_map.nos = 0;
    /* init readyqueue structure */
    ultl_info("init READYQUEUE... \n");
    g_ultl_core.ready_queue.start = NULL;
    g_ultl_core.ready_queue.end = NULL;
    g_ultl_core.ready_queue.nos = 0;
    /* init scheduler structure */
    ultl_info("init SCHEDULER... \n");
    g_ultl_core.scheduler.current_thread = NULL;
    g_ultl_core.scheduler.inthread = FALSE;
    g_ultl_core.scheduler.ininterrupt = 0;
    g_ultl_core.scheduler.type = PRIORITY;
    g_ultl_core.scheduler.RR_sleep_time = D_RR_TIME;
    g_ultl_core.scheduler.clock_pid = 0;
    g_ultl_core.scheduler.clock_pstack = NULL;
    
    /* init event reff map structure */
    ultl_info("init EVENTS REFF MAP... \n");
    g_event_map.list = NULL;
    g_event_map.nos = 0;
    /* init interrupt reff map */
    ultl_info("init st_interrupt REFF MAP... \n");
    while(count < MAX_INTERRUPT){
        g_interrupt_map.array[count].id = count + 1;
        g_interrupt_map.array[count].list = NULL;
        g_interrupt_map.array[count].nos = 0;
        count++;    
    }
    /* init key reff map structure */
    ultl_info("init KEY REFF MAP... \n");
    g_key_map.array = NULL;
    g_key_map.nos = 0;        
    /* set global init ver */
    ult_init_ver = TRUE;
    
    return SUCCESS;
}

/* _create_signal_handler(): signal handler body for new thread 
 * sig :   send signal number
 *
 * internal function for signal handler which will be called to 
 * new signal stack while creating thread 
 */ 
static void _create_signal_handler(int sig) {

    st_ulthread *ref_current = NULL;   /* stores the current_thread ref */

    ref_current = g_ultl_core.scheduler.current_thread;
    printf("Current thread: %x \n", ref_current);
    /* check is current ref in NULL */
    if(ref_current == NULL) {
        ultl_error("Current is NULL (Thread creation failed)");
        return;
    }
    /* save context of the newborn thread 
     * this code only executes at creation of a thread */
    if(setjmp(ref_current->buf)) {
        g_ultl_core.scheduler.inthread = TRUE;
        /* call the thread body and pass the args */        
        ref_current->fptr(ref_current->args);
        ref_current->exit_status = SUCCESS;            
        ref_current->state = TERMINATE;
        /* check whether there exists at two threads in readyqueue
         * if threre call thread scheduler */    
        if(g_ultl_core.ready_queue.nos > 1) {
            /* advance readyqueue and unlink current_thread*/
            g_ultl_core.ready_queue.start = g_ultl_core.ready_queue.start -> next;
            g_ultl_core.ready_queue.start->prev = NULL;
            ref_current->next = NULL;
            ref_current->prev = NULL;
            /* decrement the thread count in ready_queue */
            g_ultl_core.ready_queue.nos--;
            /* call thread scheduler */
            _thread_scheduler();                
        }
        /* if no thread in ready queue go for mainthread last saved 
         * context */
        else {
            /* set cuurent_thread as NULL */
            g_ultl_core.scheduler.current_thread = NULL;
            /* set inthread as FALSE */
            g_ultl_core.scheduler.inthread = FALSE;       
            /* unlink current_thread from ready_queue */ 
            g_ultl_core.ready_queue.nos = 0;
            g_ultl_core.ready_queue.start = NULL;
            g_ultl_core.ready_queue.end = NULL;
            ref_current->next = NULL;
            ref_current->prev = NULL;
            /* jump to the last saved main context */ 
            longjmp(g_ultl_core.scheduler.main, D_LJ_RETURN);        
        }      
        /* NOTE: we can't free the stack memory in the sig_handler
         *       as the sig_handler had been called to the allocted 
         *       stack. It would be freed when the thread is freed 
         * */      
    }
    else {
        ultl_info("Thread succesfully created.. \n");
    }
}

/* _new_thread_node(): create new thread node 
 * t_funptr : pointer to the function to be used as thread body
 * t_args:    arguments to be passed to the thread
 * return:    on success: returns the thread ref node reference, 
 *            on failure: returns NULL
 *
 * internal function allocates and initializes thread map node 
 * and thread node
 */ 
static st_trefnode *_new_thread_node(void(*fun_ptr)(void*), void *args) {

    st_trefnode *newnode = NULL;    /* ref of newnode */

    /* allocate memory for ref node */
    newnode = (st_trefnode*)ultl_malloc(sizeof(st_trefnode));    
    if(newnode == NULL) {  
         return NULL;
    }  
    /* allocate memory for newnode */
    newnode->thread = (st_ulthread*)ultl_malloc(sizeof(st_ulthread));
    if(newnode->thread == NULL) {
         ultl_free(newnode);    
         return NULL; 
    }
    /* initialize thread with default parameter */
    newnode->thread->fptr = fun_ptr;
    newnode->thread->args = args;
    /* update thread reference count */
    newnode->thread->id = ++(g_ultl_core.thread_ref_map.nos);
    newnode->thread->next = NULL;
    newnode->thread->prev = NULL;
    newnode->thread->priority = D_PRIORITY;
    newnode->thread->u_priority = D_PRIORITY;
    newnode->thread->lock_owned = INVALID;
    newnode->thread->exit_status = -D_EXITSTATUS;        
    newnode->thread->state = NEW;    
    /* return new node */
    return newnode;
}

/* ult_create(): create user level thread
 * t_funptr : pointer to the function to be used as thread body
 * t_args:    arguments to be passed to the thread
 * return:    on success: returns the thread id, 
 *            on failure: returns Error Code
 *
 * creates user level thread and returns the thread id.
 * created thread need to be explicitly started by user.
 */      
int ult_create(void(*t_funptr)(void*), void *t_args) {    

    struct sigaction handler;       /* new sig_handler to be set for thread (used to set signal stack) */
    struct sigaction oldHandler;    /* user sig_handler need to be restored for current signal stack */ 
    stack_t oldStack;               /* holds the user signal stack */
    st_trefnode * temp_node = NULL; /* to hold temp_node from thread ref arrary */  
    st_trefnode * new_node = NULL;  /* to hold the newly created thread */
    int ret = FAILURE;
   
    temp_node = g_ultl_core.thread_ref_map.array;
    new_node = _new_thread_node(t_funptr, t_args);
    /* Check is node creation fails */
    if(new_node == NULL) {
        ultl_error("Memory allocation failed for node");
        return -E_MEMALLOC;
    }
 
    /* set current as new_node */   
    g_ultl_core.scheduler.current_thread = new_node->thread;
    
    /* create the new signal stack */ 
    new_node->thread->stack.ss_flags = SS_ONSTACK;
    /* check arcitecture for stack size */
    if(sizeof(size_t) > 4){ 
        new_node->thread->stack.ss_size = ULT_STACK_SIZE_64BIT;
        ultl_info("64 bit arcitecture, stack size: %d bytes \n", ULT_STACK_SIZE_64BIT);
    }
    else if(sizeof(size_t) > 2){
       // new_node->thread->stack.ss_size = ULT_STACK_SIZE_32BIT;
        ultl_info("64 bit arcitecture, stack size: %d bytes \n", ULT_STACK_SIZE_32BIT);
    }
    else{
        new_node->thread->stack.ss_size = ULT_STACK_SIZE_16BIT;
        ultl_info("64 bit arcitecture, stack size: %d bytes \n", ULT_STACK_SIZE_16BIT);
    }    
    /* Allocate memory for stack */
    //new_node->thread->stack.ss_sp = ultl_malloc(new_node->thread->stack.ss_size);
    new_node->thread->stack.ss_size = 1024*1024; 
    new_node->thread->stack.ss_sp = ultl_malloc(1024*1024);
    
    if(new_node->thread->stack.ss_sp == NULL) { 
        ultl_error("Memory allocation failed for stack");
        ultl_free(new_node->thread);
        ultl_free(new_node);
        return -E_MEMALLOC;
    }
    /* fill stack with 1's */
   // memset(new_node->thread->stack.ss_sp, 1, new_node->thread->stack.ss_size);         
    /* Install the new stack for the signal handler 
     * stack direction is autometically managed..( ha ha ki moja !!! ) */
    if((ret = sigaltstack(&new_node->thread->stack, &oldStack)) != 0) {
        ultl_error("signal stack alteration failed");
        ultl_free(new_node->thread);
        ultl_free(new_node);
        return -E_STACKCREATE;     
    } 
  
    /* Install the signal handler */
    /* Sigaction --must-- be used so we can specify SA_ONSTACK */
    handler.sa_handler = &_create_signal_handler;
    /* set flags:   SA_ONSTACK -> to call on specified stack
     *              SA_NODEFER -> to make SIGUSR1 available inside thread
     */
    handler.sa_flags = SA_ONSTACK | SA_NODEFER;
    /* empty mask set */
    sigemptyset(&handler.sa_mask);
    /* set sig action */ 
    if((ret = sigaction(ULTL_SIGNAL, &handler, &oldHandler)) != 0) {
        ultl_error("signal action alteration failed");
        ultl_free(new_node->thread);
        ultl_free(new_node);
        return -E_STACKCREATE;     
    } 
        
    /* Raise signal to call the handler on the new stack */ 
    raise(ULTL_SIGNAL);    
        
    /* Restore old stack and handler for user ( ata kora khub dorkar ) */
    if((ret = sigaltstack(&oldStack, NULL)) != 0) { 
        ultl_error("signal stack restore failed");
        ultl_free(new_node->thread);
        ultl_free(new_node);
        return -E_STACKCREATE;     
    }
    /* restore old signal handler for user */
    if((ret = sigaction(ULTL_SIGNAL, &oldHandler, 0)) != 0) {
        ultl_error("signal action alteration failed");
        ultl_free(new_node->thread);
        ultl_free(new_node);
        return -E_STACKCREATE;    
    }
    /* Set current as NULL */
    g_ultl_core.scheduler.current_thread = NULL;        

    /* check if array is empty */
    if(temp_node!=NULL) {    
        /* traverse until end */
        while(NOT_LAST(temp_node)) {
           temp_node = temp_node->next;
        }
        /* add new_node at the end */
        temp_node->next = new_node;       
    }
    else {
        /* add first element to reference array */
        g_ultl_core.thread_ref_map.array = new_node;
    }

    /* return new thread id */       
    printf("Thread ref :: %p \n", new_node->thread);
    return new_node->thread->id;
}


/* ult_exit():  Terminates current thread and saves return status 
 * status:      exit status of the thread
 * return:      On success returns 0
                else returns error code
 * it terminates the thread from which it is called and return the status
 * given by user 
 */
int ult_exit(int status) {

    int ret = SUCCESS;                     /* the return value */
    st_ulthread * ref_current = NULL;      /* holds current thread  reference */
  
    ref_current = g_ultl_core.scheduler.current_thread;
    /* check if in thread as it should be called from a thread */ 
    if(g_ultl_core.scheduler.inthread == TRUE) {

        /* update exit status */
        ref_current->exit_status = status;
        /* set state as ABORT */
        ref_current->state = ABORT;
        /* update ready queue */
        g_ultl_core.ready_queue.start = g_ultl_core.ready_queue.start -> next;
        if(g_ultl_core.ready_queue.start != NULL) {    
            g_ultl_core.ready_queue.start -> prev = NULL;
        }
        g_ultl_core.ready_queue.nos--;   
        /* call scheduler if ready queue has more thread */ 
        if(g_ultl_core.ready_queue.nos > 0) {
            _thread_scheduler();
        }
        /* no thrdead in readyqueue so execute main */
        else {            
            /* set current as NULL */
            g_ultl_core.scheduler.current_thread = NULL;   
            g_ultl_core.scheduler.inthread = 0;     
            /* update ready queue */   
            g_ultl_core.ready_queue.nos = 0;
            g_ultl_core.ready_queue.start = NULL;
            g_ultl_core.ready_queue.end = NULL;
            /* jump to last saved main context */
            longjmp(g_ultl_core.scheduler.main, D_LJ_RETURN);        
        }
    }    
    else {
        ultl_error("\n\t Should be called from a thread !\n");
        /* set return as FAILURE */
        ret =  FAILURE;
    }        
    /* return status (This function can only return on FAILURE) */
    return ret;
}


/* _get_event_ref_byid():  get event ref by passing event id
 * @event_id :   id of the event
 * @return :     on success returns event ref
 *               on failure retunrs null
 */
static st_event* _get_event_ref_byid(int event_id) {
    
    st_event *temp_event = NULL;          /* temp event ref */  

    /* set event ref array */
    temp_event = g_event_map.list; 
    if(temp_event != NULL) {
        return temp_event;
    }
    /* traverse till end on event ref array 
     * until the event ref got found */      
    while(temp_event != NULL){
        if(temp_event->id == event_id) {
            break;        
        }
        temp_event = temp_event->next;    
    }
    /* return status */
    return temp_event;
}


/* ult_get_stackspace_() : get free space in specified thread stack
 * @thread_id :  id of the thread
 * @return :     on success return the aprox stack space remains
 *               in the thread stack
 *               on failure returns -1
 *
 * Used for debugging purpose. Returns only unused(untouched) space
 * in stack. 
 */
int ult_get_stackspace_(int thread_id){
   
   int byte_counter = FAILURE;            /* counter for free byte */
   st_ulthread *thread_ref = NULL;   /* ref of the threadc */

   /* get the reference of the thread */
   thread_ref = _get_ref_byid(thread_id);
   if(thread_ref != NULL){
       byte_counter = 0;
       while(byte_counter < thread_ref->stack.ss_size){
           /* check if any bit is changed due to call in stack then break */
           if(*((char*)(thread_ref->stack.ss_sp + byte_counter)) != 1){
               break;
           }
           /* increment byte counter */
           byte_counter++;
       }
   }
   return byte_counter;
}


/* print_waiting_threads_() : prints the thread's ID waiting for the event which
 *                            is specified by the ID
 * @event_id : ID of the event
 * @return   : On success returns the no of thread is waiting
 *             On failure returns -1
 *
 * Mainly used for Debugging purpose. Prints the thread in the waiting state for
 * the event specified by the event ID
 */ 
int print_waiting_threads_(int event_id) {

    st_event * event_ref = NULL;               /* reference of the event */
    st_trefnode * temp_thread_ref;             /* temp reference of thread ref node */
    int ret_val = FAILURE;

    /* get the reference of the event by it's ID */        
    event_ref = _get_event_ref_byid(event_id);
    /* check if event ID is valid */
    if(event_ref != NULL) {
    
        temp_thread_ref = event_ref->array;
        /* prints the waiting thread */
        while(temp_thread_ref != NULL) {
            ultl_info(" (%d | %d) ->",temp_thread_ref->thread->id,temp_thread_ref->thread->priority);    
            temp_thread_ref = temp_thread_ref->next;            
        }
        ultl_info(" (NULL)\n");
        ret_val =  event_ref->nos;        
    }    
    else {
        ultl_error("wrong event id");                    
    }
    /* return value */
    return ret_val;
}


/* ult_create_event(): creates an event for which threads will be waiting
 * @return :    on success returns event id
 *              on failure returns -1
 * 
 * thread can be sent to waiting state until a event occurs. For a single
 * event multiple thread can wait. Event is recognised by the event id
 * returned.
 */
int ult_getevent(void){

    st_event *new_event = NULL;     /* new event node ref */
    st_event *temp_event = NULL;    /* temp ref for event */
    
    /* allocate memory for event */ 
    new_event = (st_event*)malloc(sizeof(st_event));
    temp_event = g_event_map.list;
    /* check if new event is NULL */
    if(new_event == NULL) {    
        ultl_error("malloc error");
        return FAILURE;
    }

    /* initialize event */
    new_event->nos = 0;    
    new_event->next = NULL;
    /* get event id */
    new_event->id = ++g_event_map.nos;
    new_event->array = NULL;

    /* add event to eventy map */ 
    /* check if it is the first element in array */
    if(g_event_map.list == NULL) {
        g_event_map.list = new_event;
    }
    /* add new elrment in the end of list */
    else {
        while(temp_event->next != NULL) {
            temp_event = temp_event->next;    
        }
        temp_event->next = new_event;    
    }

    /* return event id */ 
    return new_event->id;
}


/* ult_wait() : send current thread to waiting state for a specified event
 * @event_id  : the ID of the event for which the therad will be waiting
 * @return    : on success return 0
 *              on failure return -1
 * 
 * Thread to which it's called will be in waiting thread ultill the 
 * waiting event being notified 
 */
int ult_wait(int event_id){

    st_event * event_ref = NULL;     /* reference of the specified event */
    st_trefnode * temp_ref = NULL;   /* temp reference to waiting thread */
    
    /* get event ref by specified event_id */
    event_ref = _get_event_ref_byid(event_id); 
    /* check if event id is valid */
    if(event_ref == NULL) {
        ultl_error("Invalid event id \n");
    }   
    /* get ref of the first thread */ 
    temp_ref = event_ref->array;

    /* check pre conditions */
    if(!g_ultl_core.scheduler.inthread){
        ultl_error("Should be called from a thread \n");
        return FAILURE; 
    } 
    if(g_ultl_core.scheduler.ininterrupt){
        ultl_error("Should not be called from a interrupt handler\n");
        return FAILURE; 
    }
   
    /* save context to of cuurent thread going to waiting state */
    if(!setjmp(g_ultl_core.scheduler.current_thread->buf)){            
        /* increment nos of waiting thread */ 
        event_ref->nos++;
        /* check if first thread to add */
        if(event_ref->array == NULL){
            event_ref->array = (st_trefnode*)malloc(sizeof(st_trefnode));
            event_ref->array->thread = g_ultl_core.scheduler.current_thread;
            event_ref->array->next = NULL;
        }
        else{
            while(temp_ref->next != NULL){
                temp_ref= temp_ref->next;
            }        
            temp_ref->next = (st_trefnode*)malloc(sizeof(st_trefnode));
            temp_ref->next->thread = g_ultl_core.scheduler.current_thread;
            temp_ref->next->next = NULL;    
        }    

        /* set core values */
        g_ultl_core.scheduler.current_thread->state = WAITING;
        /* remove current thread from ready queue */
        g_ultl_core.ready_queue.start = g_ultl_core.ready_queue.start->next;
        if(g_ultl_core.ready_queue.start != NULL)                
            g_ultl_core.ready_queue.start->prev = NULL;
        g_ultl_core.ready_queue.nos--;

        /* go for scheduling new thread or jump to main */
        if(g_ultl_core.ready_queue.nos > 0) {    
            /* call scheduler to schedule new thread */
            _thread_scheduler();
        }
        else {
            g_ultl_core.scheduler.current_thread = NULL;
            g_ultl_core.scheduler.inthread = 0;        
            g_ultl_core.ready_queue.nos = 0;
            g_ultl_core.ready_queue.start = NULL;
            g_ultl_core.ready_queue.end = NULL;
            /* jump to main context */
            longjmp(g_ultl_core.scheduler.main, D_LJ_RETURN);        
        }
    }
    /* return success after successfully coming back from wait state */
    return SUCCESS;
}


/* ult_notify() : notify threads waiting for a event to bring 
 *                in ready state
 * @event_id    : ID of the event to notify the threads
 * @return      : on success return no of thread was waiting
 *                on failure return -1
 * 
 * brings thread to ready state from waiting state, waiting for 
 * event specified by event id
 */
int ult_notify(int event_id) {
    
    st_event * event_ref = NULL;
    st_trefnode * temp_thread_ref = NULL;
    int thread_nos = 0;

    /* get reference of the event */
    event_ref = _get_event_ref_byid(event_id);
    if(event_ref == NULL){
        ultl_error("Invalid event id");    
        
    }
    /* get thread ref array */
    temp_thread_ref = event_ref->array; 
    /* get no of thread waiting */
    thread_nos = event_ref->nos;
    /* if no thread is waiting */
    if(thread_nos == 0){
	/* return with current thread nos */
        return thread_nos;
    }
    /* bring threads to ready queue */ 
    while(temp_thread_ref != NULL){
        _bring_in_readyq(temp_thread_ref->thread->id);
        temp_thread_ref = temp_thread_ref->next;
    }
    /* reset event variables */
    event_ref->nos = 0;
    event_ref->array = NULL;
    /* check if called from thread */
    if(!g_ultl_core.scheduler.inthread){
         /* save main context and schedule threads */
        if(!setjmp(g_ultl_core.scheduler.main)){
             _thread_scheduler();
        }
        else{
             g_ultl_core.scheduler.inthread = 0;
        }
    }
    /* return no of thread was waiting for the event */    
    return thread_nos;    
}


/* _get_ref_byid() : to get address of the thread of specified thread id
 * @thread_id :       id of the thread
 * @return    :       on success returns the reference of the thread
 *                    on failure returns the NULL
 * 
 * internal interface to get reference by the thread id
 */
static st_ulthread *_get_ref_byid(int thread_id){
   
    st_trefnode *temp = NULL;        /* temp variable to iterate on reference map */
    st_ulthread *thread_ref = NULL;  /* variable to store ref node */

    /* get the start of ref array */
    temp = g_ultl_core.thread_ref_map.array;
    /* traverse till the end untill the tid can be found */
    while(temp != NULL) {
        /* check if thread id same */
        if(temp->thread->id == thread_id) {
                /* ref found andd break from loop */
                thread_ref = temp->thread;
                break;
        }
        /* advance temp reference */
        temp = temp->next;
    }
    /* return thread reference */
    return thread_ref;
}


/* _bring_in_readyq(): it puts the thread in readyqueue
 * @thread_id : Id of the thread
 * @return    : on success return 0
 *              else returns error code
 * 
 * Internal function used to bring (link) a thread in the readyqueue
 * thead id is passed, tells which thread to bring in.
 */
static int _bring_in_readyq(int thread_id) {

    st_ulthread * thread_ref = NULL;       /* ref of thread */
    
    /* get the reference of the thread by id */
    thread_ref = _get_ref_byid(thread_id);
    /* check if thread is invalid */
    if(thread_ref == NULL) {
        ultl_error("\n\tNo thread reff found for tid\n");
        return -E_INVALID;
    }
    /* set thread state as READY as it is ready to be dispatched
     * by scheduler */   
    thread_ref->state = READY;
    /* check if no item in readyqueue */
    if(g_ultl_core.ready_queue.start == NULL) {
         g_ultl_core.ready_queue.start = thread_ref;
         g_ultl_core.ready_queue.end = thread_ref;
    }
    /* add ready queue to the tail of the readyqueue list */
    else {
        g_ultl_core.ready_queue.end->next = thread_ref;
        thread_ref->next = NULL;
        thread_ref->prev = g_ultl_core.ready_queue.end;
        g_ultl_core.ready_queue.end = thread_ref;
    }
    /* increment the nos of element in readyqueue */
    g_ultl_core.ready_queue.nos++;    
    return SUCCESS;
}


/* _readyq_priority_sort(): sort the readyqueue in a FCFS priority basis. 
 *                          Thread with same priority is scheduled in a FCFS manner.
 *                          A special check that prevent the start data insertion in case 
 *                          it is called from main 
 */
static int _readyq_priority_sort(void) {        

    st_ulthread *swap;
    st_ulthread *temp1;
    st_ulthread *temp2;
    st_ulthread *run;
    st_ulthread *temp;
    int no = 0;
    int i = 0;
    int j = 0;

    /* get the start of the ready queue isolated from main queue */
    run = g_ultl_core.ready_queue.start; 
    /* advance the start */
    temp = g_ultl_core.ready_queue.start->next; 
    g_ultl_core.ready_queue.start = temp;
    no = g_ultl_core.ready_queue.nos - 1;
                
    /* Do bubble sort on the rest of the queue */
    /* check if no more thread (one thread) */
    if(temp == NULL) {
      	/* for one element */
        g_ultl_core.ready_queue.start = run;      
        g_ultl_core.ready_queue.end = run;
    }
    else {
        /* check if one more thread (two thread) */
        if(temp->next==NULL) {
	    /* for two element */
            temp->prev=NULL;            
            temp->next=NULL;
            g_ultl_core.ready_queue.start=temp;
            g_ultl_core.ready_queue.end=temp;
        }
        /* check if more than two thread */ 
        else { 
            /* iterate over the queue and sort according the priority */
            for(i = 0;i < no - 1;i++) {
                 temp1 = temp;
                 temp2 = temp->next;
                 
                 for(j = 0;j < (no - 1 - i); j++) {
	              /* check all the next node */
                      if(temp2->priority > temp1->priority) {
                           /* for changing the starting thread */
		       	   if(temp1 == temp) {
                               temp = temp2;
                           }
                           /* for swaping of positions of doubly linked list */
                           temp1->next = temp2->next;
			   /* for last thread */ 
                           if (temp2->next != NULL) {
                                temp2->next->prev=temp1;
                           }
                           temp2->next=temp1;
                           temp2->prev=temp1->prev;
                           temp1->prev->next=temp2;
                           temp1->prev=temp2;
                           /* for swaping of doubly linked list*/
                           /*for swaping the name for next iteration */
                           swap=temp1;
                           temp1=temp2;
                           temp2=swap;
                           /*for swaping the name*/
                           /*for saving the last small value as queue end*/
                           if(j==no-2) {
                               g_ultl_core.ready_queue.end=temp2;
                               g_ultl_core.ready_queue.end->next=NULL;
                           }
                           /*for saving the last small value as queue end*/
                       }
                       /*increasing the next value of two pointers temp1 & temp2*/
                       temp1=temp1->next;
                       if (temp2->next==NULL){}//for last thread
                       else {
                          temp2=temp2->next;
                       }
                       /*increasing the next value of two pointers temp1 & temp2*/
                }
            } 
            g_ultl_core.ready_queue.start=temp;
            g_ultl_core.ready_queue.start->prev=NULL;
        }
        temp=g_ultl_core.ready_queue.start;
        /*showing the sorted data*/
        print_ready_queue_();
        /*insertion of the running thread which is saved as run*/
        if(run->priority > g_ultl_core.ready_queue.start->priority) {
            printf("Code hit run: %d | %d\n\n", run->id,run->priority);
            run->prev = NULL;
            g_ultl_core.ready_queue.start->prev = run;
            run->next = g_ultl_core.ready_queue.start;
            g_ultl_core.ready_queue.start = run;
        }
        else if(run->priority <= g_ultl_core.ready_queue.end->priority) {
            run->next=NULL;
            g_ultl_core.ready_queue.end->next=run;
            run->prev=g_ultl_core.ready_queue.end;
            g_ultl_core.ready_queue.end=run;
        }
        else {
            temp = temp->next;
            while(temp != g_ultl_core.ready_queue.end->next) {
                if(g_ultl_core.scheduler.inthread) {    
                     if(run->priority > temp->priority) {
                         temp->prev->next=run;
                         run->prev=temp->prev;
                         temp->prev=run;
                         run->next=temp;
                         break;
                     }
                     temp=temp->next;
                }
                else {
                     if(run->priority >= temp->priority) {
                         temp->prev->next=run;
                         run->prev=temp->prev;
                         temp->prev=run;
                         run->next=temp;
                         break;
                     }
                     temp=temp->next;
                }
            }
        }            
        /*insertion of the running thread which is saved as run*/
   }
   return 0;
}

/* ult_start(): It starts execution 'nos' of thread from 'thread_array'
 * thread_array: array holding threads ids
 * nos: nos of thread to be started  
 * return: on success return 0
 *         else returns error code
 * 
 * brings nos of thread in readyqueue, and schedules for execution 
 */
int ult_start(int *thread_array, int nos) {

    int counter = 0;                 /* counter for iteration over thread_array */    
    int new_count = 0;               /* counts the no of new thread */
    st_ulthread *thread_ref = NULL;  /* temp ver to store thread_ref */
    int ret = SUCCESS;               /* return value */
    int t_id = INVALID;              /* temp thread id */ 

    /* check if thread_array is NULL or Nos of thread is zero */
    if(thread_array == NULL || nos <= 0) {
         ultl_error("Invalid args passed by user");
         return -E_INVALPARAM;
    }
    /* iterate over thread_array and check whether that can be scheduled */
    for(counter = 0; counter < nos; counter++) {        
        t_id = thread_array[counter];
        /* get the thread ref by thread id */
        thread_ref = _get_ref_byid(t_id);
        printf("Thread ref: %p \n", thread_ref);
        /* check if thread reference not found or invalid thread id*/
        if(thread_ref == NULL) {
            ultl_error("Invalid thread id: %d at pos: %d", t_id, counter);
            ret = -E_INVALID;
            continue;
        }
        /* check if thread is already started */
        if(thread_ref->state != NEW) {    
            ultl_info("Thread of id: %d has already started",t_id);
            continue;
        }
        /* bring the thread in readyqueue for scheduling */
        ret = _bring_in_readyq(t_id);
        if(ret < 0) {
            ultl_error("Failed to start thread of id: %d - ERROR_CODE: %d", t_id, ret);
            continue;
        }
        /* increment new thread count */     
        new_count++;      
    }
    ultl_info("new Thread count: %d", new_count);
    /* XXX */ /*printreadyqueue();*/
    /* check if thread is sporned inside a scheduler handler 
     * In that case it will not schedule out current thread
     * only makes new threads ready for execution and return
     */
    if(g_ultl_core.scheduler.ininterrupt == TRUE) { 
        /* return status */       
        return ret;   
    }  
    /* check if any new thread to start*/
    if(new_count <= 0) {
        /* return status */    
        return ret;
    }
    ultl_info("thread starts from (0: main and 1: thread): %d\n", g_ultl_core.scheduler.inthread);
    /* check if code started from main */
    if(g_ultl_core.scheduler.inthread == FALSE) {
        /* save main context and call thread scheduler */
        if(!setjmp(g_ultl_core.scheduler.main)) {
            _thread_scheduler();
        }
        else {
             /* return by longjump */
        }
    } 
    /* thread is started from another thread */       
    else {
        /* save current thread context and call scheduler */
        if(!setjmp(g_ultl_core.scheduler.current_thread->buf)) {
             _thread_scheduler();
        }
        else {       
             /* return by longjump */
        }  
    }
    /* return status */
    return ret;
}

/* set_ult_priority() : used to set priority of a thread 
 * thread_id      : id of the thread 
 * priority       : the priority of the thread (1 - 10)
 * return         : on success returns 0
 *                  on failure returns error code
 * priority of a thread specified by thread_id can be 
 * changed at runtime by set_ult_priority. It can be called   
 * from inside or outside of the thread 
 * */
int set_ult_priority(int thread_id, int priority) {
    
    st_ulthread *thread = NULL;     /* ref of the thread of thread_id */

    /* check if priority value is valid */
    if(priority > 10 && priority < 1) {    
        ultl_error("Wrong priority value");
	return -E_INVALPRIORITY;
    }
    /* get reference of the thread */
    thread = _get_ref_byid(thread_id); 
    /* check if thread_id is valid */ 
    if(thread == NULL) {
        ultl_error("Wrong thread id");
        return -E_INVALID;
    }
    /* set priority value */
    thread->u_priority = priority;
    thread->priority = priority;
    /* return status */
    return SUCCESS;
}


/* ult_get_priority() : used to get priority of a thread 
 * thread_id      : id of the thread 
 * return         : on success returns thread priority
 *                  on failure returns -1
 * get priority of a specified thread by thread id. It can be called   
 * from inside or outside of the thread 
 * */
int ult_get_priority(int thread_id) {

    st_ulthread *thread = NULL;       /* reference of the thread */
    int ret = FAILURE;                /* ret value */

    /* get the reference of the thread */
    thread = _get_ref_byid(thread_id);
    if(thread != NULL) {
        ret = thread->u_priority;
    }
    /* ref not found */
    else {
        ultl_error("Wrong thread id");    
        ret = INVALID;    
    }
    /* return status */
    return ret;
}


/* ult_get_state_() : Get state of the thread specified by thread Id 
 * @thread_id    :  Id of the thread
 * @return       :  On success state of the thread 
 *                     (NEW, READY, RUNNING, WAITING,TERMINATE, ABORT)
 *                  On failure return -1    
 * 
 * Used for debugging purpose. Get the state of the thread which is 
 * still not freed 
 * */
int ult_get_state(int thread_id) {

    st_ulthread * thread = NULL;      /* ref of the thread */
    int ret = INVALID;                /* ret value for state */

    /* get the reference of the thread by Id */
    thread = _get_ref_byid(thread_id);
    /* check if thread is valid */
    if(thread != NULL) {
        /* set ret as thread current state */
        ret = thread->state;
    }
    else {
        ultl_error("Wrong thread id");    
        ret = INVALID;    
    }
    /* return status */
    return ret;
}


/* ult_get_tid() : get current active thread id
 * return    :    on success return current thread id
 *                on failure return -1
 * returns the current running thread id. Should be called
 * within the thread which id is required
 * */
int ult_get_tid() {
    
    st_ulthread *ref_current = NULL;   /* reference of the thread */
    int ret = INVALID;         /* return value */
    
    /* getting the reference of the current thread */
    ref_current = g_ultl_core.scheduler.current_thread;
    /* check if called from inside of a thread and current reference
     * is not NULL */  
    if(g_ultl_core.scheduler.inthread && ref_current != NULL) {
	/* set return as current thread id */    
        ret = ref_current->id;
    }    
    else {
        ultl_error("Should be called from a thread\n");
    }
    /* return status */
    return ret;
}


/* ult_yeild(): function to schedule out current thread
 * 
 * function used to schedule out current thread and dispatch new 
 * thread after rescheduling. Should be called within a thread 
 */
void ult_yeild(void) {

    st_ulthread *ref_current = NULL;    /* current thread reference */	
    
    /* check if called from inside of thread */
    if(!g_ultl_core.scheduler.inthread) {
        ultl_error("Should be called from a thread");
	return;
    }
    /* check if called inside of an interrupt */
    if(g_ultl_core.scheduler.ininterrupt) {
        ultl_error("should not be called inside an interrupt");
        return;
    }
    /* get current thread reference */
    ref_current = g_ultl_core.scheduler.current_thread;
    /* set current thread state as READY */
    printf("Ref current: %p r nos: %d\n", ref_current, g_ultl_core.ready_queue.nos);
    ref_current->state = READY;
    /* save current context of the thread and set jump */ 
    if(!setjmp(ref_current->buf)) {
        /* check if readyqueue has more than one thread 
	 * call scheduler to schedule threads in ready queue */
        if(g_ultl_core.ready_queue.nos > 1) {
            printf("Going to schedule thread \n");
            _thread_scheduler();
        }
	/* jump to last saved main context if no thread in readyqueue */
        else {
             /* set inthread as 0 */
             g_ultl_core.scheduler.inthread = FALSE;
	     /* set current thread ref as NULL */
             g_ultl_core.scheduler.current_thread = NULL;
	     /* jump to last saved main cotext */
             longjmp(g_ultl_core.scheduler.main, D_LJ_RETURN);
        }              
    }
    else {
            /* return to previously saved thread environment */
    }
}


int get_exit_status(int tid)
{
    st_ulthread * thread = _get_ref_byid(tid);
    if(thread != NULL)
    {
        if(thread->state == TERMINATE || thread->state == ABORT)
        {
            return thread->exit_status;
        }
        else
        {
            ultl_info("\n\tERROR: thread not terminated !!\n");
        }
    }
    else
    {
        ultl_info("\n\tERROR: thread id not there !!\n");    
    }
    return -999;
}


/* _get_key_ref_byid() : to get the reference of the key specified by ID
 * @keyid : ID of the key
 * return : on success return reference of the key
 *          on failure return NULL
 *
 * It is a internal function which is used to get reference of a key by
 * its key ID
 */
static st_key * _get_key_ref_byid(int keyid){
 
    st_key * temp_key_ref = g_key_map.array;

    while(temp_key_ref != NULL){
        if(temp_key_ref->id == keyid){
            break;
        }
        temp_key_ref = temp_key_ref->next;
    }
    return temp_key_ref;        
}


/* get_key()  : To get a key for a specified lock
 * @lock_type : Type of the lock (Soft Lock | Hard Lock)
 *              Soft Lock (Safe Lock): Deadlock never happens
 *              Hard Lock : Deadlock may happen
 * @return:     In success return the key id for a specific lock
 *              In failure -1
 */              
int get_key(int lock_type) {

    st_key * newkey = NULL;          /* new key node */
    st_key * temp = g_key_map.array; /* start of the key list */   

    /* allocate memory for the key */    
    newkey = (st_key*)malloc(sizeof(st_key));
    /* check if memory allocation fails */
    if(newkey == NULL) {
        ultl_error("Malloc error !");
        return -1;
    }
    /* check if lock type if valid */
    if(lock_type != SOFT_LOCK && lock_type != HARD_LOCK){
        ultl_error("Invalid lock type !");
        return -1;
    }
    /* intialize key */
    newkey->thread_id = INVALID;
    newkey->lock_status = FALSE;
    newkey->lock_type = lock_type;
    /* ID starts from 1 */
    newkey->id = ++g_key_map.nos;        
    newkey->next = NULL;
    /* add it to key list */
    if(temp != NULL) {
       while(temp->next != NULL) {
           temp = temp->next;
       }
       temp->next = newkey;             
    }
    else {
       g_key_map.array = newkey;            
    }
    /* return key id */
    return newkey->id;
}


/* ult_lock(): lock a particuler lock with a specified key 
 * @key_id   : Key Id of the lock
 * @return   : On success return 0 (It may never return if deadlock)
 *             On failure return -1
 */
int ult_lock(int key_id) {

   st_key *ref_current = NULL;       /* reference of the key */ 
   st_ulthread *owner_thread = NULL; /* reference of the current thread owner */

   /* check if called from ISR */
   if(g_ultl_core.scheduler.ininterrupt) {
       ultl_error("Csn't be called from ISR !\n");
       return FAILURE;       
   }

   /* get the reference of the key by it's id */
   ref_current = _get_key_ref_byid(key_id);
   /* check if key_id is valid */   
   if(ref_current == NULL) {          
       ultl_error("Invalid Key id !!");    
       return FAILURE;  
   } 
  
   /* try to lock in loop until aquired (if already lock then yeild to execute other thread) */
   while(TRUE) {
       /* if not already aquired then get it */
       if(ref_current ->lock_status == FALSE) {
           ref_current->lock_status = TRUE;
           ref_current->thread_id = ult_get_tid();   
           /* set lock owned info in thread */
           g_ultl_core.scheduler.current_thread->lock_owned = key_id;   
           return SUCCESS;    
       }
       /* if aquired then yeild so that owner thread could execute */
       else {
           ultl_info("Already locked by: %d",ref_current->thread_id);
           /* as high priority process gets executed, so if owner thread has 
           * lesser priority it never gets executed. To avoid the deadlock and
           * busy looping owner thread priority incremented one more than the 
           * current thread's priority in case if a SOFT_LOCK. 
           */
           if(ref_current->lock_type == SOFT_LOCK){
               /* get reference of the owner thread */
               owner_thread = _get_ref_byid(ref_current->thread_id);
               /* check if ref is valid */
               if(owner_thread == NULL){
                   ultl_critical("Lock owner thread ref not found");
                   exit(-1);
               }
               /* check if owner thread priority is lees than or equal of the current thread */
               if(owner_thread->priority <= g_ultl_core.scheduler.current_thread->priority){ 
                   printf("Current thread (%d) priority: %d, Owner thread (%d) priority: %d \n", 
                          g_ultl_core.scheduler.current_thread->id, g_ultl_core.scheduler.current_thread->priority, 
                                      owner_thread->id, owner_thread->priority);
                   /* set owner's priority one more than the current thread priotrity */
                   owner_thread->priority = g_ultl_core.scheduler.current_thread->priority;
               }
           }
           /* yeild to schedule other thread */
           ult_yeild();
       }
   }
   return FAILURE;
}


/* ult_unlock(): unlock a particuler lock (which was locked befor) with a 
 *               specified key
 * @key_id     : Id of the key
 * @return     : On success return 0
 *               On failure -1
 */ 
int ult_unlock(int key_id){

    st_key *ref = NULL;         /* reference of the key */
    st_ulthread *thread = NULL; /* reference of the owner thread */
    int current_tid = INVALID;  /* ID of the current thread */

    /* check if called from ISR */
    if(g_ultl_core.scheduler.ininterrupt) {
        ultl_error("Could not be called from ISR");
        return FAILURE;
    }
 
    /* get the reference of the key */
    ref = _get_key_ref_byid(key_id);
    /* check if Invalid key */       
    if(ref == NULL) {
        ultl_error("Invalid key id");
        return FAILURE;
    }
    
    /* get current thread id */
    current_tid = ult_get_tid();
    /* check if called from owner thread */    
    if(ref->thread_id != current_tid) {
        ultl_error("Owner thread should  unlock \n");
        return FAILURE;
    }
    
    /* check if lock is locked */
    if(ref->lock_status != TRUE) {
        ultl_error("Should be locked before unlock\n");
        return FAILURE;
    }

    /* get owner thread reference */
    thread = _get_ref_byid(ref->thread_id);
    /* restore to original priority of owner thread */
    thread->priority = thread->u_priority;
    /* Invalided lock owned information */
    g_ultl_core.scheduler.current_thread->lock_owned = INVALID;   
    ref->lock_status = FALSE;
    ref->thread_id = INVALID;        
    
    return SUCCESS;
}


/* print_ready_queue_() : prints the threads currently in thre readyqueue
 *                        i.e. in ready state
 * @return : On success returns the no of thread in ready state
 *           On failure return -1
 * 
 * Used for debugging purpose. In includes currently running thread also. 
 * So the count also includes the currently running thread
 */
int print_ready_queue_(void) {

    st_ulthread * temp_thread_ref = NULL;   /* temp reference of the thread ref node */
   
    /* get the first thread ref node */
    temp_thread_ref = g_ultl_core.ready_queue.start;
    /* check if core is initialzed */
    if(ult_init_ver == 0){
        ultl_error("ULTL Core is unstable or not initialised");
        return FAILURE;
    }
    /* prints the thread info */ 
    printf("START -> ");    
    while(temp_thread_ref != NULL) {
        printf(" ( %p| %d | %d ) ->", temp_thread_ref, temp_thread_ref->id, temp_thread_ref->priority);    
        temp_thread_ref = temp_thread_ref->next;
    }
    printf("END\n");
    /* return the nos of thread in ready queue */
    return g_ultl_core.ready_queue.nos;
}


#if 0

/* ult_create_interrupt(): Register a interrupt service routine to current 
 *                         thread. Should be called from a thread
 * @service_routine :   Function pointer of the service routine
 * @interrupt_id    :   The id of the interrupt 
 * @return          :   on success returns 0
 *                       on failure returns -1
 */   
int ult_create_interrupt(void (*service_routine)(int), int interrupt_id) {

    st_interrupt_thread *new_thread_node = NULL;  /* new thread info node */ 
    st_interrupt_thread *temp_thread_ref = NULL;  /* temp thread info ref */
    st_interrupt temp;                    /* temp node to hold interrupt node */
    int current_thread_id = ult_get_tid();        /* current thread id */
    
    /* check if valid interrupt id */
    if(interrupt_id < MAX_INTERRUPT && interrupt_id > 1){
        temp = g_interrupt_map.array[interrupt_id - 1];
    }    
    else {
        ultl_error("Wrong interrupt value");
        return FAILURE;
    }
    
    /* check thread list of specified interrupt id */
    temp_thread_ref = temp.list;
    /* check if no thread and service_routine is registered */
    if(temp_thread_ref != NULL){
        /* traverse untill last */
        while(TRUE){
            /* check if current thread is already registered */
            if(temp_thread_ref->thread_id == current_thread_id){
                ultl_info("replacing previous service routine by current");
                /* replace the service routine */
                temp_thread_ref->service_routine = service_routine;
                return SUCCESS;
            }
            /* check if not the last */ 
            if(NOT_LAST(temp_thread_ref)){
                temp_thread_ref = temp_thread_ref->next;
            }
            else 
                break;
        }
    }

    /* create new node and add details */
    new_thread_node = (st_interrupt_thread*)malloc(sizeof(st_interrupt_thread));
    if(new_thread_node == NULL) {        
        ultl_error("Malloc error");
        return FAILURE;
    }
    new_thread_node->thread_id = current_thread_id;
    new_thread_node->service_routine = service_routine;
    new_thread_node->next = NULL; 

    /* add node to the link list */
    if(temp_thread_ref == NULL){
        temp.list = new_thread_node;
    }
    else {
        temp_thread_ref->next = new_thread_node;
    }            
    
    /* incremant the associated thread nos for specified interrupt */
    temp.nos++;
  
    return SUCCESS;
}


int ult_raise_interrupt(int interrupt_id) {
  
}

/* ult_interrupt_thread() : to send specified interrupt to a specified thread
 * @interrupt_id : ID of the interrupt 
 * @thread_id    : ID of the thread
 * @return       : On success 0 is returned
 *                 On failure Error code is returned
 *
 * Call to provide interrupt sending mechanism. Interrupting a thread results 
 * an ISR to be called, if registered to the specified thread
 */
int ult_interrupt_thread(int interrupt_id, int thread_id) {
     
	st_ulthread *thread_ref = NULL;   /* original thread ref */
	st_ulthread *current_thread_ref = NULL; /* current thread ref */
	st_interrupt *interrupt_ref = NULL;   /* interrupt node ref */
	st_interrupt_thread *interrupt_thread_ref = NULL;  /* ISR registered thread ref */
	int poped_interrupt_ctid = INVALID;   /* poped interrupt called thread id */
	int flag_thread_found = 0;   /* registered hread found flag */
	int current_thread_id = INVALID;
	int lj_ret = 0;
	
	
	/* check if interrupt id is valid */
	if(interrupt_id > MAX_INTERRUPT || interrupt_id < IN_1) {
	    ultl_error("Invalid interrupt ID");
		return -E_INVAL_INTR_ID;
	}
	/* check if thread ref is valid */
	thread_ref = _get_ref_byid(thread_id);
	if(thread_ref == NULL) {
	    ultl_error("Invalid thread id"); 
		return -E_INVAL_THRD_ID;
	}
	/* Get the interrupt reference */
	interrupt_ref = g_interrupt_map.array[interrupt_id - 1];
	/* check if any ISR is registered forb the thread */
    /* get the interrupt thread node as first of the list*/
    interrupt_thread_ref = interrupt_ref.list;
    /* check if no thread and service_routine is registered */
    if(interrupt_thread_ref != NULL){
        /* traverse untill last */
        while(TRUE){
            /* check if current thread is already registered */
            if(interrupt_thread_ref->thread_id == current_thread_id){
                ultl_info("ISR is registered for interrupt <%u> and Thread Id <%u>", interrupt_id, thread_id);
                flag_thread_found = 1;
				break;
            }
            /* check if not the last */ 
            if(NOT_LAST(interrupt_thread_ref)){
                interrupt_thread_ref = temp_thread_ref->next;
            }
            else 
                break;
        }
    }
	/* check if thread is not found (No ISR was registered)*/
	if(!flag_thread_found) {
        ultl_info("Thread of ID <%u> received interrupt <%u> : undefined operation", thread_id, interrupt_id);
        return SUCCESS;
	}	
	else {
	    /* check if called from main */
		if(!g_ultl_core.scheduler.inthread) {		    
			/* push new interrupt node to pre_interrupt_stack.
		     * for main called_thread_id is passed as 0. */
			_push_interrupt_to_stack(interrupt_id, thread_id, 0);
			/* set the current_interrupt_id as current */
			g_interrupt_map.current_interrupt_id = interrupt_id;
			/* setting the in_service_routine flag of the interrupt thread ref */
			interrupt_thread_ref->in_service_routine = 1;
			/* save main context and continue */
			if(lj_ret = !setjmp(g_ultl_core.scheduler.main, 0)) {
				/* get original thread buf for context switching */
				/* set current_thread ref in scheduler */
				g_ultl_core.scheduler.current_thread = thread_ref;
				/* set inthread flag in scheduler */
				g_ultl_core.scheduler.inthread = 1;
				/* set ininterrupt flag in scheduler */
				g_ultl_core.scheduler.ininterrupt = 1;
				/* set state of the thread */
				thread_ref.state = INTERRUPT;
				/* ISR_LJ_RElTURN stats that the context is switched for 
				 * execution of ISR of the thread */
				longjmp(thread_ref->buf, ISR_LJ_RElTURN);
			}
			/* return for interrupt service routine execution */
			else if(lj_ret == ISR_LJ_RElTURN){
			    /*XXX*/
				
			}
			else {
				/* pop the top of the pre_interrupt_stack */
				poped_interrupt_ctid = _pop_interrupt_from_stack();
				/* check if pop fials */
				if(poped_interrupt_ctid == INVALID) {
				    ultl_critical("interrupt module error: pope failed in interrupt stack");
				    /* perform abort operation */
				    _ultl_abort();
				}
				/* check if stack is not empty */
                if(g_interrupt_map.pre_interrupt_stack.nos > 0 || g_interrupt_map.pre_interrupt_stack.top != NULL) {
                    ultl_critical("interrupt module error: main should be the last in stack");
				    /* perform abort operation */
				    _ultl_abort();
                }				
				/* unset the in_service_routine flag of the interrupt thread ref */
                interrupt_thread_ref->in_service_routine = 0;
                /* unset the current_interrupt_id as returned from main */
				g_interrupt_map.current_interrupt_id = 0;
				/* it should done on the terget thread side */
				/* XXX */
					/* unset inthread flag in scheduler as returning to main */
					g_ultl_core.scheduler.inthread = 0;
					/* unset ininterrupt flag in scheduler (as main should be the last) */
					g_ultl_core.scheduler.ininterrupt = 0;
					/* set current thread as NULL */
					g_ultl_core.scheduler.current_thread = NULL;
				/* XXX */
                /* check yeild flag for performing context switching */
                /* XXX */				
			}			
		}
		/* called from another thread */
		else {
		    current_thread_id = ult_get_tid();
		    /* check if thread_id is same as self id */
			if(thread_id == current_thread_id) {
			    /* push new interrupt node to pre_interrupt_stack.
				 * for self called_thread_id is passed as current_thread_is. */
				_push_interrupt_to_stack(interrupt_id, thread_id, current_thread_id);
				/* set the in_service_routine flag of the interrupt thread ref */
				interrupt_thread_ref->in_service_routine = 1;
				/* save the current state of the tergeted thread */
				interrupt_thread_ref.t_state = interrupt_thread_ref.state;
				/* set the state of tergeted thread */
				interrupt_thread_ref.state = INTERRUPT;
				/* set ininterrupt flag in scheduler */
				g_ultl_core.scheduler.ininterrupt = 1;
				/* call ISR of the current thread */
                interrupt_thread_ref->service_routine(interrupt_id);
				/* unset ininterrupt flag in scheduler */
				g_ultl_core.scheduler.ininterrupt = 1;
				/* restore the state of tergeted thread */
				interrupt_thread_ref.state = interrupt_thread_ref.t_state;
				interrupt_thread_ref.t_state = INVALID;
				/* unset the in_service_routine flag of the interrupt thread ref */
				interrupt_thread_ref->in_service_routine = 0;
				/* pop the top of the pre_interrupt_stack */
                _pop_interrupt_from_stack();		
			}
			else {
				/* push new interrupt node to pre_interrupt_stack.
				 * for self called_thread_id is passed as current_thread_is. */
				_push_interrupt_to_stack(interrupt_id, thread_id, current_thread_id);
				/* set the current_interrupt_id as current */
				g_interrupt_map.current_interrupt_id = interrupt_id;
				/* setting the in_service_routine flag of the interrupt thread ref */
				interrupt_thread_ref->in_service_routine = 1;
				/* get current thread ref by id */
				current_thread_ref = _get_ref_byid(current_thread_id);
				/* save current thread context and continue */
				if(!setjmp(current_thread_ref->buf, 0)) {
					/* get original thread buf for context switching */
					/* set current_thread ref in scheduler */
					g_ultl_core.scheduler.current_thread = thread_ref;
					/* set inthread flag in scheduler */
					g_ultl_core.scheduler.inthread = 1;
					/* set ininterrupt flag in scheduler */
					g_ultl_core.scheduler.ininterrupt = 1;
					/* set state of the thread */
					thread_ref.state = INTERRUPT;
					/* ISR_LJ_RElTURN stats that the context is switched for 
					 * execution of ISR of the thread */
					longjmp(thread_ref->buf, ISR_LJ_RElTURN);
				}
				else {
					/* pop the top of the pre_interrupt_stack */
					poped_interrupt_ctid = _pop_interrupt_from_stack();
					/* check if pop fails */
					if(poped_interrupt_ctid == INVALID) {
						ultl_critical("interrupt module error: pope failed in interrupt stack");
						/* perform abort operation */
						_ultl_abort();
					}
					/* check if poped the current thread */
					if(poped_interrupt_ctid != current_thread_id) {
					    ultl_critical("interrupt module error: pope thread id not matched with current thread id");
						/* perform abort operation */
						_ultl_abort();
					}
					/* check if stack is empty */
					if(g_interrupt_map.pre_interrupt_stack.nos == 0 && g_interrupt_map.pre_interrupt_stack.top == NULL) {
						/* unset ininterrupt flag in scheduler */
						g_ultl_core.scheduler.ininterrupt = 0;
						/* it should done on the terget thread side */
						g_interrupt_map.current_interrupt_id = 0;
					}
                    else {
						/* unset ininterrupt flag in scheduler */
						g_ultl_core.scheduler.ininterrupt = 1;
						/* it should done on the terget thread side */
						g_interrupt_map.current_interrupt_id = /*<get from current stack top>*/;
					}
					/* unset the in_service_routine flag of the interrupt thread ref */
					interrupt_thread_ref->in_service_routine = 0;
					/* check yeild flag for performing context switching */
					/* XXX */				
				}
			}
		}
	}
}

#endif

/* ult_scheduler() : to select scheduler type on runtime 
 * @type : Type of the scheduler
 * @return : On success returns 0
 *           On failure returns ERROR CODE
 *
 * selecting different scheduler type actually changes the scheduler type at 
 * runtime. The default scheduling type is non preeemptive priority scheduling 
 * (NPPRIORITY). There is basically three types of scheduler is available:
 * 1> FCFS scheduling (FCFS)
 * 2> Non Preemptive priority scheduling (NPPRIORITY => FCFS | PRIORITY)
 * 3> Round Robin Scheduling (RR => FCFS | RR)
 * 4> Round Robin Priority Scheduling (PRIORITYRR => PRIORITY | RR)
 */
int ult_scheduler(int type) {

    int current_stype = g_ultl_core.scheduler.type; /* current scheduler type */
    int c_id = getpid();
    int clock_pid = getpid();
    void *clock_pstack = NULL;
    struct sigaction handler;
   
    /* check scheduler type and take action acccordingly */
    switch(type) {
        case RR: 
        if(current_stype == RR){
            ultl_info("\n\ttype not chenged\n");
            return SUCCESS;
        }
        else {
            handler.sa_handler = &_RR_schedule_handler;
            handler.sa_flags = 0;
            sigemptyset(&handler.sa_mask);
            sigaction(ULTL_SIGNAL, &handler, NULL);
            clock_pstack = malloc(CLOCK_STACK_SIZE);
            if(clock_pstack == NULL) {
                ultl_error("Clock stack creation failed\n");
                return -ESCH_STACKC;
            }
            clock_pid = clone(_RRscheduler_clock, clock_pstack, CLONE_VM, NULL);
            if(clock_pid <= 0) {
                ultl_error("clone failed - continuing with prev scheduler\n"); 
                free(clock_pstack);
                return -ESCH_CLONEF;
            }
            g_ultl_core.scheduler.type = RR;
            g_ultl_core.scheduler.clock_pid = clock_pid;            
            g_ultl_core.scheduler.clock_pstack = clock_pstack;            
        }
        break;

        case FCFS:
        if(current_stype == FCFS){
            ultl_info("\n\ttype not chenged\n");
            return SUCCESS;
        }
        else {

 
        }
        break;
/*
        case PRIORITY:
        if(current_stype == NPPRIORITY){
            ultl_info("\n\ttype not chenged\n");
            return SUCCESS;
        }
        else {

 
        }
        break
 
        case PRIORITYRR:
        if(current_stype == PRIOTITYRR){
            ultl_info("\n\ttype not chenged\n");
            return SUCCESS;
        }
        else {

 
        }
        break;
*/    
        default:
        ultl_info("\n\tUnknown Type\n");
        return FAILURE;
        
    }
    return 0;
}

int ult_free(int tid)
{
    st_ulthread * thread = _get_ref_byid(tid);
    st_trefnode * tnode;
    st_trefnode * tpnode = NULL;
    st_trefnode * temp = g_ultl_core.thread_ref_map.array;    
    st_trefnode * prev = NULL;    
    st_trefnode * freenode;
    st_event * enode;
    if(thread != NULL)
    {
        if(thread->state == NEW || thread->state == TERMINATE || thread->state == ABORT)
        {
            while(temp!= NULL)
            {    
            if(temp->thread == thread)
            {
                if(prev == NULL)
                {
                    freenode = temp;                
                    g_ultl_core.thread_ref_map.array = temp->next;
                    free(freenode->thread->stack.ss_sp);
                    free(freenode->thread);                
                    free(freenode);                
                    return 0;
                }
                else
                {
                    freenode = temp;
                    prev->next = temp->next;
                    free(freenode->thread->stack.ss_sp);
                    free(freenode->thread);
                    free(freenode);
                    return 0;                    
                }
            }
            prev = temp;
               temp = temp->next;            
            }
            return -1;
        }
        else if(thread->state == READY)
        {
                if(g_ultl_core.ready_queue.nos > 1)
            {
                if(thread->prev == NULL)
                  {
                g_ultl_core.ready_queue.start = thread->next;
                g_ultl_core.ready_queue.start->prev = NULL;
                }
                else if(thread->next == NULL)
                {
                thread->prev->next = NULL;
                g_ultl_core.ready_queue.end = thread->prev;
                }
                else
                {
                thread->prev->next = thread->next;
                thread->next->prev = thread->prev;
                }
                    g_ultl_core.ready_queue.nos--;
            }
            else
            {
                g_ultl_core.ready_queue.start = NULL;
                g_ultl_core.ready_queue.end = NULL;
                g_ultl_core.ready_queue.nos = 0;    
             }
            while(temp!= NULL)
            {    
            if(temp->thread == thread)
            {
                if(prev == NULL)
                {
                    freenode = temp;                
                    g_ultl_core.thread_ref_map.array = temp->next;
                    free(freenode->thread->stack.ss_sp);
                    free(freenode->thread);                
                    free(freenode);                
                    return 0;
                }
                else
                {
                    freenode = temp;
                    prev->next = temp->next;
                    free(freenode->thread->stack.ss_sp);
                    free(freenode->thread);
                    free(freenode);
                    return 0;                    
                }
            }
            prev = temp;
               temp = temp->next;            
            }
                    return -1;
        }
        else if(thread->state==WAITING)
        {
            enode = g_event_map.list;  
            while(enode!=NULL)
            {
            tnode = enode->array;
            while(tnode != NULL)
            {    
                if(tnode->thread == thread)
                {
                    if(tpnode == NULL)
                    {
                        enode->array = tnode->next;
                        free(tnode);                    
                    }
                    else
                    {
                        tpnode->next = tnode->next;                
                        free(tnode);                    
                    }
                    enode->nos--;
                }
                tpnode = tnode;
                   tnode = tnode->next;            
            }
            }
            while(temp != NULL)
            {    
            if(temp->thread == thread)
            {
                if(prev == NULL)
                {
                    freenode = temp;                
                    g_ultl_core.thread_ref_map.array = temp->next;
                    free(freenode->thread->stack.ss_sp);                
                    free(freenode->thread);                
                    free(freenode);                
                    return 0;
                }
                else
                {
                    freenode = temp;
                    prev->next = temp->next;
                    free(freenode->thread->stack.ss_sp);
                    free(freenode->thread);
                    free(freenode);
                    return 0;                    
                }
            }
            prev = temp;
               temp = temp->next;            
            }
            return -1;
        }
        else if(thread->state == RUNNING)
        {
                ultl_error("\n\tError: Running Thread can't be freed\n");
            return 1;
        }
    }
    else
    {
        ultl_info("\n\tERROR: thread id not there !!\n");    
    }
    return -1;    
}

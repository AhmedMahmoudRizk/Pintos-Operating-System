#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/fixed_point.h"

#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/****************************************/
static struct list sleeping_list;
fixed_point load_avg;
fixed_point parent_recent_cpu;
int parent_nice;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/* Idle thread. */
static struct thread *idle_thread;
list_less_func *less =&list_less;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;


/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void) 
{
  //printf("111111");
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&all_list);
  /******************************************/
  list_init(&sleeping_list);

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();

  init_thread (initial_thread, "main", PRI_DEFAULT);

  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) 
{
  struct thread *t = thread_current ();
  
  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;

  thread_ticks++;
  if(thread_current() != idle_thread) /* increament recent cpu for running thread if not idle */
    thread_current()->recent_cpu = add_int_to_FP_num(1,thread_current()->recent_cpu);
  if(check_suspend_queue() || thread_ticks >= TIME_SLICE){ 
  //wake sleeping if wake time has passed,of thread reached time slice
    intr_yield_on_return ();
  }

}

/* Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  //printf("1\n");
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;
  enum intr_level old_level;

  ASSERT (function != NULL);
  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;
/********************************************************************/
  /* 
  creator (current thread) is the parent so we set variables of the parent 
  with it to be inherited by the newly created thread
  */
  parent_recent_cpu = thread_current()->recent_cpu;
  parent_nice = thread_current()->nice;
/********************************************************************/
  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();

  /* Prepare thread for first run by initializing its stack.
     Do this atomically so intermediate values for the 'stack' 
     member cannot be observed. */
  old_level = intr_disable ();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  intr_set_level (old_level);

  /* Add to run queue. */
  thread_unblock (t);
  /***********************************************************************/
  /* preemtion test is needed as we add thread to ready_list (may have priority > running thread)*/
  preemption_test();
  return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
  ASSERT (t->status == THREAD_BLOCKED);
  list_push_back (&ready_list, &t->elem);
  t->status = THREAD_READY;
  
  intr_set_level (old_level);

}

/* Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current()->allelem);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{

  struct thread *cur = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread) 
    {
        list_push_back (&ready_list, &cur->elem);
    }
  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) 
{
/* 
  sets original priority of thread ,
  sets it's effective if current thread holding no locks 
  (means that no thread is waiting or donating his priority to current thread) 
  and if current effective priority is less than the new one
  -- after it we test preemtion because we set changed the priority
*/
  struct thread *cur =  thread_current ();
  if(!thread_mlfqs){
    cur->priority = new_priority;
    if(cur->effective_priority < new_priority || list_empty(&cur->holded_locks))
      thread_current ()->effective_priority = new_priority;
    preemption_test();
  }

}

void thread_set_mlfqs_priority(struct thread *t,void *aux UNUSED){
  
    fixed_point term1 = divide_FP_num_by_int(4,t->recent_cpu);
    int term2 = 2*t->nice;
    fixed_point sub = add_int_to_FP_num(term2,term1);
    t->priority = PRI_MAX - FP_num_to_int_trunc(sub);
    
/* adjust priority to be in [0 : 63] */
    if(t->priority < 0) 
      t->priority = 0;
    else if(t->priority > 63)
      t->priority = 63;
    t->effective_priority=t->priority;

  }
/* Returns the current thread's priority. */
int
thread_get_priority (void) 
{
  /* return actual priority which should be effective at a time */
  return thread_current ()->effective_priority; 
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int nice) 
{
  struct thread *cur =  thread_current ();
  if(cur != idle_thread){
    cur->nice=nice;
    thread_set_mlfqs_priority(cur,NULL);
    preemption_test();
  }
}
/* Sets the current thread's recent_cpu value to NICE. */
void
thread_set_recent_cpu (struct thread *t , void *aux UNUSED) 
{
  if(t != idle_thread){
  fixed_point term1 = add_int_to_FP_num(1,multiply_int_and_FP_num(2,load_avg));
  fixed_point term2 = divide_FP_nums(multiply_int_and_FP_num(2,load_avg),term1);
  fixed_point term3 = multiply_FP_nums(t->recent_cpu,term2);
  t->recent_cpu = add_int_to_FP_num(t->nice,term3);
  }
}
/* Sets the current thread's load_avg value to NICE. */
void
thread_set_load_avg (int l_avg UNUSED) 
{
  int ready_threads = list_size(&ready_list) + (thread_current() != idle_thread);
  fixed_point term1 = int_to_FP_num(59);
  fixed_point term2 = divide_FP_num_by_int(60,term1);
  fixed_point term3 = multiply_FP_nums(term2,load_avg);
  fixed_point term4 = int_to_FP_num (1);
  fixed_point term5 = divide_FP_num_by_int(60,term4);
  fixed_point term6 = multiply_int_and_FP_num(ready_threads,term5);          
  load_avg =  add_FP_nums(term6,term3);  
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) 
{
  return thread_current()->nice;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) 
{
  return FP_num_to_int_round(multiply_int_and_FP_num(100,load_avg));
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) 
{
  return FP_num_to_int_round(multiply_int_and_FP_num(100,thread_current()->recent_cpu));
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;) 
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
    //printf("7\n");

  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;

  /***************************************************************/
  if(!thread_mlfqs)
    t->priority = priority;
  else{
    if(t == initial_thread){ //initialize attributes of intitial thread (grandfather)
      t->recent_cpu = 0;
      t->nice = 0;
      parent_recent_cpu = 0;
      parent_nice = 0;
      load_avg = 0;
    }
    /* inheritence of attributes of any newly created thread */
    t->nice = parent_nice; 
    t->recent_cpu = parent_recent_cpu;
    thread_set_mlfqs_priority(t,NULL); //calculate mlfqs priority for the first time 
  }  
  t->effective_priority = priority;
  t->waited_lock = NULL;      
  list_init(&t->holded_locks); 
  /*****************************************************************/
  t->magic = THREAD_MAGIC;
  list_push_back (&all_list, &t->allelem);

}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) 
{
  if (list_empty (&ready_list))
    return idle_thread;
  else{
        int compare_type=compare_effective_max;
        struct list_elem *max = list_max((&ready_list), less, &compare_type);
        list_remove(max);
        return list_entry (max,struct thread, elem);
  }
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();
  
  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();

#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void) 
{
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1;
  tid_t tid;
  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);

/* add current thread to list of sleeping threads and sets it's wake time */
void
thread_suspend (int64_t wake_time) 
{

  ASSERT (!intr_context ());
  enum intr_level old_level;
  old_level = intr_disable ();
  struct thread *current_thr=thread_current();
  current_thr->wake_time=wake_time;
  int compare_flag = compare_wake_time;


  list_insert_ordered(&sleeping_list,&current_thr->elem,less,&compare_flag);
  current_thr->status = THREAD_SLEEPING;
  
  schedule ();
  intr_set_level(old_level);//very important interrupt disabling for schedular 

}
/* 
  check sleeping threads list to wake threads reached or passed their waking time 
  adding them to ready list no need to call scheduler here because this func is called inside 
  the timer interrupt (intr_yield_on_return is called if any thread is awaken)
  that's the reason it's a boolean function
  if wake time of least of the list is > current ticks we break
*/
bool
check_suspend_queue(void){
  enum intr_level old_level;
  old_level = intr_disable ();

  struct list_elem *e;
  bool wake=false;
  
  // interrupt disabled because we are looping we don't want interruption

   if(list_size(&sleeping_list)==0)
    return false;
  for (e = list_begin (&sleeping_list); e != list_end (&sleeping_list);)
    {
      struct thread *t = list_entry (e, struct thread, elem);
      if(t->wake_time > (kernel_ticks+user_ticks+idle_ticks) ||  t->wake_time==0)
         break;
     
      t->status=THREAD_READY;
      t->wake_time=0;
      wake=true;
      e = list_next (e);
      list_push_back (&ready_list, list_pop_front(&sleeping_list));
    }
    intr_set_level(old_level);
  return wake;
}
void preemption_test(void){
    enum intr_level old_level;
    old_level = intr_disable ();
    int compare_type = compare_effective_max;
    /* handle preemption test in first lock release of (tid lock) of main thread */
    if(!list_empty(&ready_list)){
    if(thread_current()->effective_priority<
      list_entry(list_max((&ready_list), less, &compare_type),struct thread,elem)->effective_priority)
      {
        thread_yield();
      }
}
     intr_set_level(old_level);//important interrupt disabling for thread_yield 
}

/* 
comparator function between threads
compare_effective_max: to campare threads by effective for list_max function (oldest max is returned)
compare_effective_ordered: to campare threads by effective for list_inseret_ordered
and sort_function (oldest max in back of the list)
compare_effective_max to campare threads by effective for list_max function (oldest max is returned)
compare_wake_time ordered
*/


bool list_less(const struct list_elem *a,const struct list_elem *b,void *aux){
  int compare_type= *(int *)aux;
  struct thread *t1=list_entry(a,struct thread ,elem);
  struct thread *t2=list_entry(b,struct thread ,elem);
  if(compare_type == compare_effective_max){
      if(t1->effective_priority<t2->effective_priority)
        return true;
  }
  else if (compare_type == compare_effective_ordered){
      if(t1->effective_priority<=t2->effective_priority)
        return true;
  }
  else if(compare_type == compare_wake_time){
    
    if(t1->wake_time<t2->wake_time)
      return true;
   }
  return false;
}



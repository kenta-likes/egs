/*
 * minithread.c:
 *      This file provides a few function headers for the procedures that
 *      you are required to implement for the minithread assignment.
 *
 *      EXCEPT WHERE NOTED YOUR IMPLEMENTATION MUST CONFORM TO THE
 *      NAMING AND TYPING OF THESE PROCEDURES.
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include "interrupts.h"
#include "minithread.h"
#include "synch.h"
#include <assert.h>
#include "unistd.h"
#include "alarm.h"

#define LOWEST_PRIORITY 3

typedef struct minithread {
  int id;
  int priority;
  int rem_quanta;
  stack_pointer_t stackbase;
  stack_pointer_t stacktop;
  int status;
} minithread;

int current_id = 0; // the next thread id to be assigned
semaphore_t id_lock = NULL;
int runnable_count = 0;
minithread_t current_thread = NULL;
multilevel_queue_t runnable_q = NULL;
semaphore_t runnable_q_lock = NULL;
queue_t blocked_q = NULL;
semaphore_t blocked_q_lock = NULL;
queue_t dead_q = NULL;
semaphore_t dead_q_lock = NULL;
semaphore_t dead_sem = NULL;
int sys_time = 0;
const int TIME_QUANTA = 100 * MILLISECOND;

//getter for priority
int minithread_priority(){
  return current_thread->priority;
}

int minithread_time(){
  return sys_time;
}

int random_number(int max_num) {
  srand(time(NULL));
  return rand() % max_num;
}

int choose_priority_level() {
  int num;
  num = random_number(100);

  if (num < 50) return 0;
  else if (num < 75) return 1;
  else if (num < 90) return 2;
  else return 3;
}

int clean_up(){
  minithread_t dead = NULL;
  while (1){
    semaphore_P(dead_sem);
    semaphore_P(dead_q_lock);
    if (queue_dequeue(dead_q, (void**)(&dead)) == -1){
      semaphore_V(dead_q_lock);
      return -1;
    }
    else {
      semaphore_V(dead_q_lock);
      minithread_free_stack(dead->stackbase);
      free(dead);
    }
  }
  return -1;
} 

int scheduler() {
  int next_priority = 0;
  minithread_t next = NULL;
  minithread_t temp = NULL;
   
  while (1) {
    set_interrupt_level(ENABLED);
    while (runnable_count == 0){};
    //dequeue from runnable threads
    next_priority = choose_priority_level();
    set_interrupt_level(DISABLED);
    if (multilevel_queue_dequeue(runnable_q,
        next_priority,(void**)(&next)) != -1) {
      runnable_count--;
      temp = current_thread;
      current_thread = next;
      minithread_switch(&(temp->stacktop),&(next->stacktop));
      return 0;
    }
    //if dead/runnable queue is empty, do nothing (idle thread)
  }
  return 0;
}

/*
 * A minithread should be defined either in this file or in a private
 * header file.  Minithreads have a stack pointer with to make procedure
 * calls, a stackbase which points to the bottom of the procedure
 * call stack, the ability to be enqueueed and dequeued, and any other state
 * that you feel they must have.
 */


int
minithread_exit(minithread_t completed) {
  current_thread->status = DEAD;
  semaphore_P(dead_q_lock);
  queue_append(dead_q, current_thread);
  semaphore_V(dead_q_lock);
  semaphore_V(dead_sem);
  scheduler();
  while(1);
  return 0;
}
 
minithread_t
minithread_fork(proc_t proc, arg_t arg) {
  minithread_t new_thread = minithread_create(proc,arg);
  
  semaphore_P(runnable_q_lock);  
  if(multilevel_queue_enqueue(runnable_q,
      new_thread->priority,new_thread) == 0) {
    runnable_count++; //add to queue
  }
  semaphore_V(runnable_q_lock);
  return new_thread;
}

minithread_t
minithread_create(proc_t proc, arg_t arg) {
  minithread_t new_thread = (minithread_t)malloc(sizeof(minithread));
  if (new_thread == NULL){
    return NULL;
  }
  
  semaphore_P(id_lock);
  new_thread->id = current_id++;
  semaphore_V(id_lock);
  new_thread->priority = 0;
  new_thread->rem_quanta = 1;
  new_thread->stackbase = NULL;
  new_thread->stacktop =  NULL;
  new_thread->status = RUNNABLE;
  minithread_allocate_stack(&(new_thread->stackbase), &(new_thread->stacktop) );
  minithread_initialize_stack(&(new_thread->stacktop), proc, arg,
                              (proc_t)minithread_exit, NULL);
  return new_thread; 
}

minithread_t
minithread_self() {
  return current_thread;
}

int
minithread_id() {
  return current_thread->id;
}

void
minithread_stop() { 
  current_thread->status = BLOCKED;
  semaphore_P(blocked_q_lock);
  queue_append(blocked_q,current_thread);
  semaphore_V(blocked_q_lock);
  scheduler();
}

void
minithread_start(minithread_t t) {
  t->status = RUNNABLE;
  t->priority = 0;
  t->rem_quanta = 1;
  
  semaphore_P(runnable_q_lock);
  if (multilevel_queue_enqueue(runnable_q,
      t->priority,t) == 0) {
    runnable_count++;
  }
  semaphore_V(runnable_q_lock);
}

/**
 * This function does need to be protected because it is only called
 * from within a semaphore P, during which interrupts are disabled.
 * */
void
minithread_enqueue_and_schedule(queue_t q) {
  current_thread->status = BLOCKED;
  queue_append(q, current_thread);
  scheduler();
}

/**
 * This function does need to be protected because it is only called
 * from within a semaphore V, during which interrupts are disabled.
 * */
void
minithread_dequeue_and_run(queue_t q) {
  minithread_t blocked_thread = NULL;
  queue_dequeue(q, (void**)(&blocked_thread) );
  if (blocked_thread->status != BLOCKED) {
    printf("thread %d should have status BLOCKED\n", minithread_id());
  }
  minithread_start(blocked_thread);
}

void
minithread_demote_priority() {
  if (current_thread->priority == LOWEST_PRIORITY);
  else current_thread->priority++;
  current_thread->rem_quanta = 1 << current_thread->priority;
  semaphore_P(runnable_q_lock);
  if (multilevel_queue_enqueue(runnable_q,
      current_thread->priority,current_thread) == 0) {
    runnable_count++;
  }
  semaphore_V(runnable_q_lock);
 
  scheduler();
}

void
minithread_yield() {
  //put current thread at end of runnable
  semaphore_P(runnable_q_lock);
  current_thread->priority = 0;
  current_thread->rem_quanta = 1;

  if (multilevel_queue_enqueue(runnable_q,
      current_thread->priority,current_thread) == 0) {
    runnable_count++;
  }
  semaphore_V(runnable_q_lock);
  //call scheduler here
  scheduler();
}

/*
 * This is the clock interrupt handling routine.
 * You have to call minithread_clock_init with this
 * function as parameter in minithread_system_initialize
 */
void 
clock_handler(void* arg) {
  interrupt_level_t l;

  l = set_interrupt_level(DISABLED);
  sys_time += 1;
  execute_alarms(sys_time);
  if (--(current_thread->rem_quanta) == 0) {
    set_interrupt_level(l);
    minithread_demote_priority();
  }
  else {
    set_interrupt_level(l);
  }
}

void
wake_up(void* sem){
  semaphore_V((semaphore_t)sem);
}

/*
 * sleep with timeout in milliseconds
 */
void 
minithread_sleep_with_timeout(int delay){
  interrupt_level_t l;
  semaphore_t thread_sem;
  int num_cycles;

  num_cycles = delay % (TIME_QUANTA/MILLISECOND) == 0? delay/(TIME_QUANTA/MILLISECOND) : delay / (TIME_QUANTA/MILLISECOND) + 1;

  thread_sem = semaphore_create();
  l = set_interrupt_level(DISABLED);
  set_alarm(num_cycles, wake_up, (void*)thread_sem, sys_time);
  set_interrupt_level(l);
  semaphore_P(thread_sem);
  semaphore_destroy(thread_sem);
}

/*
 * Initialization.
 *
 *      minithread_system_initialize:
 *       This procedure should be called from your C main procedure
 *       to turn a single threaded UNIX process into a multithreaded
 *       program.
 *
 *       Initialize any private data structures.
 *       Create the idle thread.
 *       Fork the thread which should call mainproc(mainarg)
 *       Start scheduling.
 *
 */
void
minithread_system_initialize(proc_t mainproc, arg_t mainarg) {
  minithread_t clean_up_thread = NULL;
  int a = 0;
  void* dummy_ptr = NULL;
  minithread_t tmp = NULL;
  tmp = NULL;
  dummy_ptr = (void*)&a;
  current_id = 0; // the next thread id to be assigned
  id_lock = semaphore_create();
  semaphore_initialize(id_lock,1); 
  runnable_q = multilevel_queue_new(4);
  blocked_q = queue_new();
  blocked_q_lock = semaphore_create();
  semaphore_initialize(blocked_q_lock,1);
  dead_q = queue_new();
  dead_q_lock = semaphore_create();
  semaphore_initialize(dead_q_lock,1);
  dead_sem = semaphore_create();
  semaphore_initialize(dead_sem,0);    
  runnable_q_lock = semaphore_create();
  semaphore_initialize(runnable_q_lock,1);
  clean_up_thread = minithread_create(clean_up, NULL);
  multilevel_queue_enqueue(runnable_q,
    clean_up_thread->priority,clean_up_thread);
  runnable_count++;
  minithread_clock_init(TIME_QUANTA, (interrupt_handler_t)clock_handler);
  init_alarm();
  current_thread = minithread_create(mainproc, mainarg);
  minithread_switch(&dummy_ptr, &(current_thread->stacktop));
  return;
}


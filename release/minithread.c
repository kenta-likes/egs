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
#include "minithread.h"
#include "synch.h"
#include <assert.h>
#include "unistd.h"

typedef struct minithread {
  int id;
  stack_pointer_t stackbase;
  stack_pointer_t stacktop;
  int status;
} minithread;


int current_id = 0; // the next thread id to be assigned

minithread_t current_thread = NULL;
minithread_t scheduler_thread = NULL;
queue_t runnable_q = NULL;
queue_t blocked_q = NULL;
queue_t dead_q = NULL;
 
int scheduler(){
  minithread_t next;
  minithread_t tmp;
  while (1){
    //check for dead threads, free them
    //dequeue from runnable threads
    if ( queue_length(runnable_q) > 0 ){
      if (queue_dequeue(runnable_q, (void**)(&next) ) == -1){
        //error
      }
      tmp = current_thread;
      current_thread = next;
      minithread_switch(&(tmp->stacktop), &( next->stacktop));
    }
    //if runnable threads is empty, idle loop
    //
  }
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
  minithread_t tmp;
  current_thread->status = DEAD;
  //call scheduler here
  queue_append(dead_q, current_thread);
  tmp = current_thread;
  current_thread = scheduler_thread;
  minithread_switch(&(tmp->stacktop), &( current_thread->stacktop));
  return 0;
}
 
minithread_t
minithread_fork(proc_t proc, arg_t arg) {
  minithread_t new_thread = minithread_create(proc,arg);
  queue_append(runnable_q, new_thread);//add to queue
  return new_thread;
}

minithread_t
minithread_create(proc_t proc, arg_t arg) {
  minithread_t new_thread = (minithread_t)malloc(sizeof(minithread));
  if (new_thread == NULL){
    return NULL;
  }
  new_thread->id = current_id++;
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
minithread_stop() { minithread_enqueue_and_schedule(blocked_q); }

void
minithread_start(minithread_t t) {
  t->status = RUNNABLE;
  queue_append(runnable_q, t);
}

void
minithread_enqueue_and_schedule(queue_t q) {
  minithread_t tmp;

  current_thread->status = BLOCKED;
  queue_append(q, current_thread);
  
  tmp = current_thread;  
  current_thread = scheduler_thread;

  // invoke scheduler
  minithread_switch(&(tmp->stacktop), &(current_thread->stacktop));
}

void
minithread_dequeue_and_run(queue_t q) {
  void* blocked_thread;
  queue_dequeue(q, &blocked_thread);

  if (((minithread_t)blocked_thread)->status != BLOCKED) {
    printf("thread %d should have status BLOCKED\n", minithread_id());
  }
  minithread_start((minithread_t)blocked_thread);
}

void
minithread_yield() {
  void* tmp = NULL;
  minithread_t prev = current_thread;

  queue_append(runnable_q, current_thread);
  queue_dequeue(runnable_q, &tmp);
  prev = current_thread;
  current_thread = (minithread_t)tmp;
  minithread_switch(&(prev->stacktop), &( ((minithread_t)tmp)->stacktop));
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
  int a;
  void* dummy_ptr;
  minithread_t tmp;
  tmp = NULL;
  dummy_ptr = (void*)&a;
  current_id = 0; // the next thread id to be assigned
  
  runnable_q = queue_new();
  blocked_q = queue_new();
  dead_q = queue_new();
  current_thread = minithread_create(mainproc, mainarg);
  scheduler_thread = minithread_create(scheduler, NULL);
  minithread_switch(&dummy_ptr, &(current_thread->stacktop));
  //minithread_switch(&(tmp->stacktop), &(current_thread->stacktop));
  while ( queue_length(runnable_q) > 0){
    //do nothing for FIFO scheduling, since
    //we assume processes voluntarily give up
    //CPU by calling yield. Once we are non-preemptive
    //we will begin adding stuff here.
  }
}


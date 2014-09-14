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
#include "queue.h"
#include "synch.h"
#include <assert.h>

typedef struct minithread {
  int id;
  stack_pointer_t stackbase;
  stack_pointer_t stacktop;
  int status;
} minithread;

int current_id = 0; // the next thread id to be assigned

minithread_t current_thread = NULL;
queue_t runnable = NULL;
 
/*
 * A minithread should be defined either in this file or in a private
 * header file.  Minithreads have a stack pointer with to make procedure
 * calls, a stackbase which points to the bottom of the procedure
 * call stack, the ability to be enqueueed and dequeued, and any other state
 * that you feel they must have.
 */


/* minithread functions */

int
minithread_exit(minithread_t completed) {
  return 0;
  /*minithread_t next = NULL;
  free(completed->stackbase);
  free(completed);
  while (1) {
    next = queue_dequeue(runnable);
    if (next != NULL) {
      //there is another process to run
  */    
}
 
minithread_t
minithread_fork(proc_t proc, arg_t arg) {
  minithread_t new_thread = minithread_create(proc,arg);
  minithread_switch(new_thread->stacktop, new_thread->stacktop);
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
  new_thread->stacktop = NULL;
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
}

void
minithread_start(minithread_t t) {
}

void
minithread_yield() {
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
  current_id = 0; // the next thread id to be assigned
  runnable = queue_new();
  current_thread = minithread_fork( mainproc, mainarg );//malloc can fail
  //scheduling below...
}


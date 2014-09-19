#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "defs.h"
#include "synch.h"
#include "queue.h"
#include "minithread.h"

/*
 *      You must implement the procedures and types defined in this interface.
 */

/*
 * Semaphores.
 */
struct semaphore {
  tas_lock_t* l;
  int cnt;    
};


void acquire_lock(tas_lock_t* l) { while(atomic_test_and_set(l) != 0); }
void release_lock(tas_lock_t* l) { atomic_clear(l); }

/*
 * semaphore_t semaphore_create()
 *      Allocate a new semaphore.
 */
semaphore_t semaphore_create() {
  semaphore_t new_sem;
  new_sem = (semaphore_t)malloc(sizeof(struct semaphore));

  // check if malloc was successful
  if (new_sem == NULL) return NULL;
  
  // initialize semaphore fields
  new_sem->l = (tas_lock_t*)malloc(sizeof(int));
  new_sem->cnt = 0;
  return new_sem; 
}

/*
 * semaphore_destroy(semaphore_t sem);
 *      Deallocate a semaphore.
 */
void semaphore_destroy(semaphore_t sem) {
  // check if sem is a valid arg
  if (sem == NULL) return;
  
  free(sem->l);
  free(sem); 
}


/*
 * semaphore_initialize(semaphore_t sem, int cnt)
 *      initialize the semaphore data structure pointed at by
 *      sem with an initial value cnt.
 */
void semaphore_initialize(semaphore_t sem, int cnt) {
  // check if sem is a valid arg
  if (sem == NULL) return;
 
  // set the lock to available
  clear_lock(sem->l);
  sem->cnt = cnt;  
}


/*
 * semaphore_P(semaphore_t sem)
 *      P on the sempahore.
 */
void semaphore_P(semaphore_t sem) {
  // keep checking whether lock is avaiable
  // if available, grab it and move on
  acquire_lock(sem->l);
  
  if (--sem->cnt < 0) {
    minithread_stop();
    release_lock(sem->l);
    minithread_yield();
  }
  else {
    release_lock(sem->l);
  }
}

/*
 * semaphore_V(semaphore_t sem)
 *      V on the sempahore.
 */
void semaphore_V(semaphore_t sem) {
  void** blocked;
  // keep checking whether lock is avaiable
  // if available, grab it and move on
  acquire_lock(sem->l);
  
  if (++sem->l >= 0) {
    release_lock(sem->l);
  }
  else {
    queue_dequeue(sem->waitq, blocked);
    minithread_start((minithread_t)(*blocked));
    release_lock(sem->l);
  } 
}


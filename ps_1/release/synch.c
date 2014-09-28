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
  tas_lock_t* lock;
  int count;
  queue_t wait_q;    
};


void acquire_lock(tas_lock_t* l) { while(atomic_test_and_set(l) != 0); }
void release_lock(tas_lock_t* l) { atomic_clear(l); }

/*
 * semaphore_t semaphore_create()
 *      Allocate a new semaphore.
 */
semaphore_t semaphore_create() {
  semaphore_t new_sem = NULL;
  queue_t new_q = NULL;
  new_sem = (semaphore_t)malloc(sizeof(struct semaphore));
  new_q = queue_new();

  // check if malloc was successful
  if (new_sem == NULL || new_q == NULL) return NULL;
  
  // initialize semaphore fields
  new_sem->lock = (tas_lock_t*)malloc(sizeof(int));
  new_sem->count = 0;
  new_sem->wait_q = new_q;
  return new_sem; 
}

/*
 * semaphore_destroy(semaphore_t sem);
 *      Deallocate a semaphore.
 */
void semaphore_destroy(semaphore_t sem) {
  // check if sem is a valid arg
  if (sem == NULL) return;
  
  free(sem->lock);
  queue_free(sem->wait_q);
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
  release_lock(sem->lock);
  sem->count = cnt;  
}

void semaphore_block(semaphore_t sem) {
  minithread_enqueue_and_schedule(sem->wait_q);
}

/*
 * semaphore_P(semaphore_t sem)
 *      P on the sempahore.
 */
void semaphore_P(semaphore_t sem) {
  acquire_lock(sem->lock);
  if (--sem->count < 0){    
    release_lock(sem->lock);
    semaphore_block(sem);
  }
  else {
    release_lock(sem->lock);
  }
}


void semaphore_unblock(semaphore_t sem) {
  minithread_dequeue_and_run(sem->wait_q);
}

/*
 * semaphore_V(semaphore_t sem)
 *      V on the sempahore.
 */
void semaphore_V(semaphore_t sem) {
  acquire_lock(sem->lock);
  if (++sem->count > 0) {
    release_lock(sem->lock);
  }
  else {
    semaphore_unblock(sem);
    release_lock(sem->lock);
  }
}

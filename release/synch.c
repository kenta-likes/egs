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
  atomic_clear(sem->l);
  sem->cnt = cnt;  
}


/*
 * semaphore_P(semaphore_t sem)
 *      P on the sempahore.
 */
void semaphore_P(semaphore_t sem) {
  // keep checking whether lock is avaiable
  // if available, grab it and move on
  while (atomic_test_and_set(sem->l) != 0);
  sem->cnt--;

  // release lock
  atomic_clear(sem->l);
}

/*
 * semaphore_V(semaphore_t sem)
 *      V on the sempahore.
 */
void semaphore_V(semaphore_t sem) {
  // keep checking whether lock is avaiable
  // if available, grab it and move on
  while (atomic_test_and_set(sem->l) != 0);
  sem->cnt++;

  // release lock
  atomic_clear(sem->l);
}

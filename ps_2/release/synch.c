#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "defs.h"
#include "synch.h"
#include "queue.h"
#include "minithread.h"
#include "interrupts.h"

/*
 *      You must implement the procedures and types defined in this interface.
 */

/*
 * Semaphores.
 */
struct semaphore {
  int count;
  queue_t wait_q;    
};

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
  new_sem->count = 0;
  new_sem->wait_q = new_q;
  return new_sem; 
}

/*
 * semaphore_destroy(semaphore_t sem);
 *      Deallocate a semaphore.
 */
void semaphore_destroy(semaphore_t sem) {
  interrupt_level_t l;

  // check if sem is a valid arg
  if (sem == NULL) return;
  
  l = set_interrupt_level(DISABLED); 
  queue_free(sem->wait_q);
  free(sem); 
  set_interrupt_level(l);
}


/*
 * semaphore_initialize(semaphore_t sem, int cnt)
 *      initialize the semaphore data structure pointed at by
 *      sem with an initial value cnt.
 */
void semaphore_initialize(semaphore_t sem, int cnt) { 
  // check if sem is a valid arg
  if (sem == NULL) return;
  
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
  interrupt_level_t l;

  l = set_interrupt_level(DISABLED);
  if (--sem->count < 0) {
    semaphore_block(sem);
  }
  set_interrupt_level(l);
}


void semaphore_unblock(semaphore_t sem) {
  minithread_dequeue_and_run(sem->wait_q);
}

/*
 * semaphore_V(semaphore_t sem)
 *      V on the sempahore.
 */
void semaphore_V(semaphore_t sem) {
  interrupt_level_t l;

  l = set_interrupt_level(DISABLED);
  if (++sem->count <= 0) {
    semaphore_unblock(sem);
  }
  set_interrupt_level(l);
}

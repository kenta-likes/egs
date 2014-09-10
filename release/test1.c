/* test1.c
   Tests the queue implementation.
   Spawn a single thread.
*/
#include "queue.h"
#include "minithread.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int
thread(int* arg) {
  printf("Hello, world!\n");

  return 0;
}

int
main(void) {
  /* int* x1 = (int*)malloc(sizeof(int));
  int* x2 = (int*)malloc(sizeof(int));
  int* x3 = (int*)malloc(sizeof(int)); 
  int* x4 = (int*)malloc(sizeof(int));
  
  *x1 = 1;
  *x2 = 2;
  *x3 = 3;
  *x4 = 4;

  queue_t q = queue_new();
  assert(queue_length(q) == 0);
  queue_append(q, x1);
  queue_dequeue(q);
  queue_prepend(q, x1);
  queue_dequeue(q);
  queue_append(q, x1);
  queue_append(q, x2);
  queue_append(q, x3);
  queue_append(q. x4); */
  
  minithread_system_initialize(thread, NULL);
  return 0;
}

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
  int* x1;
  int* x2;
  int* x3;
  int* x4;
  queue_t q;
  void** item;
  
  x1 = (int*)malloc(sizeof(int)); 
  x2 = (int*)malloc(sizeof(int)); 
  x3 = (int*)malloc(sizeof(int)); 
  x4 = (int*)malloc(sizeof(int));
  item = (void**)malloc(sizeof(void*));  
  *x1 = 1;
  *x2 = 2;
  *x3 = 3;
  *x4 = 4;
  printf("initialized variables, starting test\n");
  q = queue_new();
  assert(queue_length(q) == 0);
  printf("created new queue, tested length = 0\n");
  queue_append(q, x1);
  queue_dequeue(q, item);
  assert(**((int**)item) == 1);
  queue_prepend(q, x1);
  queue_dequeue(q, item);
  queue_append(q, x1);
  queue_append(q, x2);
  queue_append(q, x3);
  printf("added three items to queue\n");
  printf("queue length is %d\n",queue_length(q));
  

  minithread_system_initialize(thread, NULL);
  return 0;
}

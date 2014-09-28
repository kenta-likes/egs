/* test0.c
   Tests the queue implementation.
*/
#include "queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

void
increment(void* x, void* y) {
  (*((int*)x))++;
}

int
main(void) {
  int* x0;
  int* x1;
  int* x2;
  int* x3;
  int* x4;
  queue_t q;
  void** item;
  
  x0 = (int*)malloc(sizeof(int));
  x1 = (int*)malloc(sizeof(int)); 
  x2 = (int*)malloc(sizeof(int)); 
  x3 = (int*)malloc(sizeof(int)); 
  x4 = (int*)malloc(sizeof(int));
  item = (void**)malloc(sizeof(void*));
  *x0 = 0;  
  *x1 = 1;
  *x2 = 2;
  *x3 = 3;
  *x4 = 4;
  
  printf("initialized variables, starting test\n");
  q = queue_new();
  assert(queue_length(q) == 0);
  printf("created new queue, tested length = 0\n");
  assert(queue_dequeue(q, item) == -1);
  if (queue_append(q, x1) == -1) {
    printf("append failed");
  }
  if (queue_dequeue(q, item)== -1) {
    printf("dequeue failed");
  }
  assert(**((int**)item) == 1);
  queue_prepend(q, x1);
  queue_dequeue(q, item);
  queue_append(q, x1);
  queue_append(q, x2);
  queue_append(q, x3);
  queue_prepend(q, x0); 
  printf("added 4 items to queue\n");
  printf("queue length is %d\n",queue_length(q));
  
  printf("incrementing all values in the queue\n");
  queue_iterate(q, increment, x1);
  queue_dequeue(q, item);
  assert(**((int**)item) == 1);
  queue_dequeue(q, item);
  assert(**((int**)item) == 2);
  queue_dequeue(q, item);
  assert(**((int**)item) == 3);
  queue_dequeue(q, item);
  assert(**((int**)item) == 4);
  printf("all values were incremented correctly\n");
   
  queue_append(q, x1);
  queue_append(q, x2);
  queue_append(q, x3);
  queue_prepend(q, x0); 
  assert(queue_delete(q, x0) == 0);
  assert(queue_length(q) == 3); // testing delete removed an item
  assert(queue_dequeue(q, item) == 0);
  assert(**((int**)item) == 2);
  assert(queue_delete(q, x1) == 0); // x1 is not in the queue anymore
                                 // but this still shouldn't raise error
  assert(queue_length(q) == 2);
  assert(queue_delete(q, x2) == 0);
  assert(queue_delete(q, x3) == 0);
  assert(queue_delete(NULL, x4) == -1);
  
  assert(queue_free(NULL) == -1);
  assert(queue_free(q) == 0);
  q = queue_new();
  queue_append(q, x1);
  queue_append(q, x2);
  assert(queue_free(q) == 0);
  
  return 0;
}

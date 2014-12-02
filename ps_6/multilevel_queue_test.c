/** 
 * test implementation of multilevel_queue.
 * **/
#include "stdlib.h"
#include "stdio.h"
#include "multilevel_queue.h"
#include "assert.h"

int main(void) {
  long x1, x2, x3, x4, val;
  multilevel_queue_t multi_q = NULL;
  val = 0;
  x1 = 1;
  x2 = 2;
  x3 = 3;
  x4 = 4;

  multi_q = multilevel_queue_new(0);
  assert(multi_q == NULL);
  multilevel_queue_free(multi_q);

  multi_q = multilevel_queue_new(1);
  multilevel_queue_enqueue(multi_q,0,(void*)x1);
  multilevel_queue_enqueue(multi_q,0,(void*)x2);
  assert(multilevel_queue_dequeue(multi_q,0,(void**)(&val)) == 0);
  assert(val == 1);
  assert(multilevel_queue_dequeue(multi_q,0,(void**)(&val)) == 0);
  assert(val == 2);
  assert(multilevel_queue_dequeue(multi_q,0,(void**)(&val)) == -1);
  multilevel_queue_free(multi_q);

  multi_q = multilevel_queue_new(2);
  // check that we can enqueue and dequeue multiple items
  multilevel_queue_enqueue(multi_q,0,(void*)x1);
  multilevel_queue_enqueue(multi_q,0,(void*)x2);
  multilevel_queue_enqueue(multi_q,1,(void*)x3);
  multilevel_queue_enqueue(multi_q,1,(void*)x4);
  // check that dequeue will wrap around
  assert(multilevel_queue_dequeue(multi_q,1,(void**)(&val)) == 1);
  assert(val == 3);
  assert(multilevel_queue_dequeue(multi_q,1,(void**)(&val)) == 1);
  assert(val == 4);
  assert(multilevel_queue_dequeue(multi_q,1,(void**)(&val)) == 0);
  assert(val == 1);
  // check that the level of the item returned is correct
  // check that failure on 
  multilevel_queue_free(multi_q);

  multi_q = multilevel_queue_new(4);  
  assert(multilevel_queue_enqueue(multi_q,4,(void*)x1) == -1);
  multilevel_queue_enqueue(multi_q,0,(void*)x1);
  multilevel_queue_enqueue(multi_q,1,(void*)x2);
  multilevel_queue_enqueue(multi_q,2,(void*)x3);
  multilevel_queue_enqueue(multi_q,3,(void*)x4);
  assert(multilevel_queue_dequeue(multi_q,2,(void**)(&val)) == 2);
  assert(val == 3);
  assert(multilevel_queue_dequeue(multi_q,3,(void**)(&val)) == 3);
  assert(val == 4);
  assert(multilevel_queue_dequeue(multi_q,1,(void**)(&val)) == 1);
  assert(val == 2);
  assert(multilevel_queue_dequeue(multi_q,1,(void**)(&val)) == 0);
  assert(val == 1);
  assert(multilevel_queue_dequeue(multi_q,1,(void**)(&val)) == -1);
  assert(val == 0);
  multilevel_queue_free(multi_q);
  
  printf("potato.\n");
  return 0;
}


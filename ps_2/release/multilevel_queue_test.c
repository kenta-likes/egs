/** 
 * test implementation of multilevel_queue.
 * **/
#include "stdlib.h"
#include "stdio.h"
#include "multilevel_queue.h"
#include "assert.h"

int main(void) {
  long x1, x2, x3, x4, x5, x6, val;
  multilevel_queue_t multi_q = NULL;
  val = 0;
  x1 = 1;
  x2 = 2;
  x3 = 3;
  x4 = 4;
  x5 = 5;
  x6 = 6;

  multi_q = multilevel_queue_new(0);
  multilevel_queue_free(multi_q);

  multi_q = multilevel_queue_new(1);
  multilevel_queue_enqueue(multi_q,0,(void*)x1);
  assert(multilevel_queue_dequeue(multi_q,0,(void**)(&val)) == 0);
  assert(val == 1);

  multilevel_queue_free(multi_q);
  printf("potato.\n");
  return 0;
}


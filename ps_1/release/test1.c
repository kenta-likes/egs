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
  minithread_system_initialize(thread, NULL);
  return 0;
}

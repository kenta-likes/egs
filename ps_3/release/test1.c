/* test1.c
   Tests the queue implementation.
   Spawn a single thread.
*/
#include "queue.h"
#include "minithread.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/** testing func pointers
typedef void (*some_type)(void*);

typedef struct some_struct{
  some_type hello;
} some_struct;

void
herro_world(void* arg){
  printf("Herro wurld\n");
}
*/

int
thread(int* arg) {
  printf("Hello, world!\n");
  minithread_sleep_with_timeout(1);
  printf("Hello, world!\n");
  minithread_sleep_with_timeout(1);
  printf("Hello, world!\n");
  minithread_sleep_with_timeout(1);
  printf("Hello, world!\n");
  minithread_sleep_with_timeout(1);
  printf("Hello, world!\n");
  minithread_sleep_with_timeout(1);
  printf("Hello, world!\n");

  return 0;
}

int
main(void) {
  minithread_system_initialize(thread, NULL);
  return 0;
}

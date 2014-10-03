/* test0.c
   Tests the queue implementation.
*/
#include "minithread.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>

int
test_scheduler_io(int* arg){
  long i;
  long count;
  count = 0;
  for (i = 0; i < LONG_MAX/10; i++){
    count += minithread_priority();
    minithread_yield();
  }
  assert((float)count / i <= 1.0);
  printf("io bound fits schedule\n");
  return 0;
}

int fork_bomb(int* lv){
  float i = 0;
  float j = .5;

  if (i == 0){
    while (1) {
      if (minithread_priority() == 3){
        printf("bananas at %li\n", (long)lv);
        break;
      }
      i += j;
      j /= 2;
    }
  } 
  minithread_fork(fork_bomb, (void*)(((long)i)-1));
  while (1) {
    if (minithread_priority() == 3){
      printf("bananas at %li\n", (long)lv);
      break;
    }
    i += j;
    j /= 2;
  }
  //while (1);
  return 0;
}
/*
int
test_scheduler_cpu(int* arg){
  float i = 0;
  float j = .5;

  //fun way to calculate 1 yerrr
  while (1) {
    if (minithread_priority() == 3){
      printf("bananas\n");
    }
    i += j;
    j /= 2;
  }
  return 0;
}
*/

int
main(void) {
  minithread_system_initialize(fork_bomb, (int*)10);
  return 0;
}

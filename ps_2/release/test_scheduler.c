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

int fork_bomb(int* i){
  while (i == 0){
    minithread_yield();
  } 
  minithread_fork(fork_bomb, (void*)(((long)i)-1));
  while (1){
    minithread_yield();
  }
  return 0;
}

int
test_scheduler_cpu(int* arg){
  //long i;
  float i = 0;
  float j = .5;
  //long count;
  //minithread_fork(fork_bomb, (void*)10);
  while (1) {
    if (minithread_priority() == 3){
      printf("bananas\n");
    }
    //printf("cpu_bound being called from priority %d.\n",minithread_priority());
    i += j;
    j /= 2;
  }
  //assert((float)count / i >=  3.0);
  return 0;
}


int
main(void) {
  minithread_system_initialize(test_scheduler_cpu, NULL);
  return 0;
}

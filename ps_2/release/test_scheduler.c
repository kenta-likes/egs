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

int fork_bomb(int i, int j){
  if (i == 0 || j == 0){
    return 0;
  } 
  return i + j + fork_bomb(i-1,j) + fork_bomb(i,j-1);
}

int
test_scheduler_cpu(int* arg){
  long i;
  long count;
  minithread_fork(test_scheduler_io, NULL);

  count = 0;
  for (i = 0; i < LONG_MAX/10; i++){
    fork_bomb( 1, 1);
    count += minithread_priority();
    minithread_yield();
  }
  assert((float)count / i >=  3.0);
  printf("cpu bound fits schedule\n");
  return 0;
}


int
main(void) {
  minithread_system_initialize(test_scheduler_cpu, NULL);
  return 0;
}

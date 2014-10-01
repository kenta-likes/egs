/* test0.c
   Tests the queue implementation.
*/
#include "alarm.h"
#include "minithread.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int sys_time;

void
print_alarm(void* arg){
  printf("Set off alarm %li\n", (long)arg);
}

int
test_alarms(int* arg){
  alarm_id tmp;
  sys_time = 0;
  init_alarm();

  tmp = set_alarm(1, print_alarm, (void*)1, sys_time);
  tmp = set_alarm(10, print_alarm, (void*)1, sys_time);
  assert(deregister_alarm(tmp) == 0);

  return 0;
}


int
main(void) {
  /**
  assert(queue_length(q) == 0);
  **/
  minithread_system_initialize(test_alarms, NULL);
  return 0;
}

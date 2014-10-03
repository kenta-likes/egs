/* test0.c
   Tests the queue implementation.
*/
#include "alarm.h"
#include "minithread.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

int sys_time

void
print_alarm(void* arg){
  printf("Set off alarm %li\n", (long)arg);
}

/*
int
test_long_alarm(int* arg){
  minithread_sleep_with_timeout(4000);
  printf("printing something\n");
  return 0;
}
*/

int
test_alarms(int* arg){
  alarm_id tmp1;
  alarm_id tmp2;
  alarm_id tmp3;
  alarm_id tmp4;
  alarm_id tmp5;
  alarm_id tmp6;
  alarm_id tmp7;
  alarm_id tmp8;
  alarm_id tmp9;
  alarm_id tmp10;
  alarm_id tmp_long;
  alarm_list_t a_list;

  sys_time = 0;
  a_list = init_alarm();


  tmp1 = set_alarm(1, print_alarm, (void*)((long)sys_time + 1), sys_time);
  tmp2 = set_alarm(10, print_alarm, (void*)1, sys_time);

  tmp1 = set_alarm(10, print_alarm, (void*)1, sys_time);//t = 10
  sys_time += 3;
  tmp2 = set_alarm(20, print_alarm, (void*)2, sys_time);//t = 23
  tmp3 = set_alarm(25, print_alarm, (void*)3, sys_time);//t = 28
  sys_time += 3;
  tmp4 = set_alarm(10, print_alarm, (void*)4, sys_time);//t = 15
  tmp5 = set_alarm(20, print_alarm, (void*)5, sys_time);//t = 26
  tmp6 = set_alarm(5, print_alarm, (void*)6, sys_time);// t = 11
  sys_time += 2;
  tmp7 = set_alarm(1, print_alarm, (void*)7, sys_time);// t = 8

  tmp8 = set_alarm(200, print_alarm, (void*)8, sys_time);// t = 207
  tmp9 = set_alarm(1000, print_alarm, (void*)9, sys_time);// t = 1007
  tmp10 = set_alarm(500, print_alarm, (void*)10, sys_time);// t = 507

  tmp_long = set_alarm(4000, print_alarm, (void*)11, sys_time);// t = 4007

  printf("add 10 alarms consecutively....SUCCESS\n");

  while (1){
  }

  free(a_list);
  return 0;
}


int
main(void) {
  minithread_system_initialize(test_long_alarm, NULL);
  return 0;
}

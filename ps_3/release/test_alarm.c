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
  int STOP = 100;
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
  alarm_list_t a_list;

  sys_time = 0;
  a_list = init_alarm();

  assert(deregister_alarm(NULL) == -1);

  printf("deregister null alarm..........SUCCESS\n");
  tmp1 = set_alarm(1, print_alarm, (void*)((long)sys_time + 1), sys_time);
  tmp2 = set_alarm(10, print_alarm, (void*)1, sys_time);
  assert(deregister_alarm(tmp2) == 0);

  printf("deregister unexecuted alarm....SUCCESS\n");
  execute_alarms(sys_time + 1);
  assert(deregister_alarm(tmp1) == 1);

  printf("deregister executed alarm......SUCCESS\n");
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
  assert(alarm_list_len(a_list) == 10);

  printf("add 10 alarms consecutively....SUCCESS\n");
  while (sys_time != STOP){
    execute_alarms(sys_time);
    sys_time++;
  }
  printf("execute 7 alarms...............SUCCESS\n");
  assert(deregister_alarm(tmp1) == 1);
  assert(deregister_alarm(tmp2) == 1);

  printf("deregister executed alarms.....SUCCESS\n");
  assert(alarm_list_len(a_list) == 3);

  printf("check remaining alarms.........SUCCESS\n");
  assert(deregister_alarm(tmp8) == 0);
  assert(deregister_alarm(tmp9) == 0);
  assert(deregister_alarm(tmp10) == 0);

  printf("deregister unexecuted alarms...SUCCESS\n");
  free(a_list);
  return 0;
}

int
main(void) {
  test_alarms(NULL);
  return 0;
}

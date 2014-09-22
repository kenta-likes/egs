/* Application
*/

#include "minithread.h"
#include "synch.h"

#include <stdio.h>
#include <stdlib.h>


semaphore_t cereal_sem; //yes, it's cereals all the way
queue_t cereal_nums; //contains all cereal nums

static int N = 30;//employees
static int M = 5000;//buyers
long curr_cereal_num;
int customers_served; //remaining customers
int phones_produced; //remaining customers

int buyer_thread(int* arg) {
  long c_num;
  semaphore_P(cereal_sem);
  queue_dequeue(cereal_nums, (void**)(&c_num));
  customers_served++;
  printf("GOT AN EGS PHONE! Cereal # is: %ld\n", c_num);
  return 0;
}

int employee_thread(int* arg) {
  long c_num;
  while (phones_produced < M) {
    semaphore_V(cereal_sem);
    phones_produced++;
    c_num = curr_cereal_num++;
    queue_append(cereal_nums, (void*)c_num);
    minithread_yield();
  }
  return 0;
}

int init_shop(int* arg){
  int i;
  for (i = 0; i < N; i++){
    minithread_fork(employee_thread, NULL);
  }
  for (i = 0; i < M; i++){
    minithread_fork(buyer_thread, NULL);
  }
  return 0;
}

int
main(int argc, char *argv[]) {
  cereal_sem = semaphore_create();
  cereal_nums = queue_new();
  customers_served = 0;
  curr_cereal_num = 0;
  phones_produced = 0;
  semaphore_initialize(cereal_sem, 0);
  minithread_system_initialize(init_shop, NULL);
  return -1;
}

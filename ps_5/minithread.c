/*
 * minithread.c:
 *      This file provides a few function headers for the procedures that
 *      you are required to implement for the minithread assignment.
 *
 *      EXCEPT WHERE NOTED YOUR IMPLEMENTATION MUST CONFORM TO THE
 *      NAMING AND TYPING OF THESE PROCEDURES.
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include "interrupts.h"
#include "minithread.h"
#include "synch.h"
#include <assert.h>
#include "unistd.h"
#include "alarm.h"
#include "network.h"
#include "minimsg.h"
#include "minisocket.h"
#include "miniheader.h"
#include "miniroute.h"
#define LOWEST_PRIORITY 3

typedef struct minithread {
  int id;
  int priority;
  int rem_quanta;
  stack_pointer_t stackbase;
  stack_pointer_t stacktop;
  int status;
} minithread;

int current_id = 0; // the next thread id to be assigned
semaphore_t id_lock = NULL;
int runnable_count = 0;
minithread_t current_thread = NULL;
multilevel_queue_t runnable_q = NULL;
queue_t blocked_q = NULL;
semaphore_t blocked_q_lock = NULL;
queue_t dead_q = NULL;
semaphore_t dead_q_lock = NULL;
semaphore_t dead_sem = NULL;
int sys_time = 0;
const int TIME_QUANTA = 100 * MILLISECOND;
network_address_t my_addr;

//getter for priority
int minithread_priority(){
  return current_thread->priority;
}

int minithread_time(){
  return sys_time;
}

int random_number(int max_num) {
  srand(time(NULL));
  return rand() % max_num;
}

int choose_priority_level() {
  int num;
  num = random_number(100);

  if (num < 50) return 0;
  else if (num < 75) return 1;
  else if (num < 90) return 2;
  else return 3;
}

int clean_up(){
  minithread_t dead = NULL;
  while (1){
    semaphore_P(dead_sem);
    semaphore_P(dead_q_lock);
    if (queue_dequeue(dead_q, (void**)(&dead)) == -1){
      semaphore_V(dead_q_lock);
      return -1;
    }
    else {
      semaphore_V(dead_q_lock);
      minithread_free_stack(dead->stackbase);
      free(dead);
    }
  }
  return -1;
} 

int scheduler() {
  int next_priority = 0;
  minithread_t next = NULL;
  minithread_t temp = NULL;
   
  while (1) {
    while (runnable_count == 0) {
      set_interrupt_level(ENABLED);
    };
    
    set_interrupt_level(DISABLED);
    //dequeue from runnable threads
    next_priority = choose_priority_level();
    if (multilevel_queue_dequeue(runnable_q,
        next_priority,(void**)(&next)) != -1) {
      runnable_count--;
      temp = current_thread;
      current_thread = next;
      minithread_switch(&(temp->stacktop),&(next->stacktop));
      return 0;
    }
    //if dead/runnable queue is empty, do nothing (idle thread)
  }
  return 0;
}

/*
 * A minithread should be defined either in this file or in a private
 * header file.  Minithreads have a stack pointer with to make procedure
 * calls, a stackbase which points to the bottom of the procedure
 * call stack, the ability to be enqueueed and dequeued, and any other state
 * that you feel they must have.
 */


int
minithread_exit(minithread_t completed) {
  current_thread->status = DEAD;
  semaphore_P(dead_q_lock);
  queue_append(dead_q, current_thread);
  semaphore_V(dead_q_lock);
  semaphore_V(dead_sem);
  scheduler();
  while(1);
  return 0;
}
 
minithread_t
minithread_fork(proc_t proc, arg_t arg) {
  interrupt_level_t l;
  minithread_t new_thread = minithread_create(proc,arg);
  
  l = set_interrupt_level(DISABLED);
  if(multilevel_queue_enqueue(runnable_q,
      new_thread->priority,new_thread) == 0) {
    runnable_count++; //add to queue
  }
  set_interrupt_level(l);
  return new_thread;
}

minithread_t
minithread_create(proc_t proc, arg_t arg) {
  minithread_t new_thread = (minithread_t)malloc(sizeof(minithread));
  if (new_thread == NULL){
    return NULL;
  }
  
  semaphore_P(id_lock);
  new_thread->id = current_id++;
  semaphore_V(id_lock);
  new_thread->priority = 0;
  new_thread->rem_quanta = 1;
  new_thread->stackbase = NULL;
  new_thread->stacktop =  NULL;
  new_thread->status = RUNNABLE;
  minithread_allocate_stack(&(new_thread->stackbase), &(new_thread->stacktop) );
  minithread_initialize_stack(&(new_thread->stacktop), proc, arg,
                              (proc_t)minithread_exit, NULL);
  return new_thread; 
}

minithread_t
minithread_self() {
  return current_thread;
}

int
minithread_id() {
  return current_thread->id;
}

void
minithread_stop() { 
  current_thread->status = BLOCKED;
  semaphore_P(blocked_q_lock);
  queue_append(blocked_q,current_thread);
  semaphore_V(blocked_q_lock);
  scheduler();
}

void
minithread_start(minithread_t t) {
  interrupt_level_t l;

  t->status = RUNNABLE;
  t->priority = 0;
  t->rem_quanta = 1;
  
  l = set_interrupt_level(DISABLED);
  if (multilevel_queue_enqueue(runnable_q,
      t->priority,t) == 0) {
    runnable_count++;
  }
  set_interrupt_level(l);
}

/**
 * This function does need to be protected because it is only called
 * from within a semaphore P, during which interrupts are disabled.
 * */
void
minithread_enqueue_and_schedule(queue_t q) {
  current_thread->status = BLOCKED;
  queue_append(q, current_thread);
  scheduler();
}

/**
 * This function does need to be protected because it is only called
 * from within a semaphore V, during which interrupts are disabled.
 * */
void
minithread_dequeue_and_run(queue_t q) {
  minithread_t blocked_thread = NULL;
  queue_dequeue(q, (void**)(&blocked_thread) );
  if (blocked_thread->status != BLOCKED) {
    printf("thread %d should have status BLOCKED\n", minithread_id());
  }
  minithread_start(blocked_thread);
}

/**
 * minithread_demote_priority is called from the clock handler.
 * Interrupts are already disabled when this function is called so mutual exclusion gauranteed.
 * This threads priority is decreased, its quanta replenished, placed back on runnable queue
 * and the scheduler is invoked.
 **/
void
minithread_demote_priority() {
  if (current_thread->priority == LOWEST_PRIORITY);
  else current_thread->priority++;
  current_thread->rem_quanta = 1 << current_thread->priority;

  if (multilevel_queue_enqueue(runnable_q,
      current_thread->priority,current_thread) == 0) {
    runnable_count++;
  }
  scheduler();
}

void
minithread_yield() {
  interrupt_level_t l;  

  //put current thread at end of runnable
  current_thread->priority = 0;
  current_thread->rem_quanta = 1;

  l = set_interrupt_level(DISABLED);
  if (multilevel_queue_enqueue(runnable_q,
      current_thread->priority,current_thread) == 0) {
    runnable_count++;
  }
  set_interrupt_level(l);
  //call scheduler here
  scheduler();
}

/*
 * This is the clock interrupt handling routine.
 * You have to call minithread_clock_init with this
 * function as parameter in minithread_system_initialize.
 * If this thread has exhausted its quanta, this its priority is decreased
 * and the scheduler is invoked. In this case, interrupts are not re-enabled in this function
 * but when the scheduler switches to another thread.
 */
void 
clock_handler(void* arg) {
  interrupt_level_t l;

  l = set_interrupt_level(DISABLED);
  sys_time += 1;
  execute_alarms(sys_time);
  if (--(current_thread->rem_quanta) == 0) {
    minithread_demote_priority();
  }
  else {
    set_interrupt_level(l);
  }
}

/*
 * shift contents of buf to the LEFT by offset bytes.
 * The number of contents shifted is denoted by
 * bytes_shifted
 * */
void shift_by_n(char* buf, int offset, int bytes_shifted){
  int i;
  
  for (i = 0; i < bytes_shifted; i++){
    buf[i] = buf[i+offset];
  }
  return;
}

/**
 *  Network handler function which gets called whenever packet
 *  arrives. Handler disables interrupts for duration of function.
 *  Puts packet onto pkt_q to be processed later by process_packets
 *  thread.
 */
void 
network_handler(network_interrupt_arg_t* pkt){
  interrupt_level_t l;
  mini_header_t pkt_hdr;
  struct routing_header* router_hdr;
  char protocol;
  char* buf_ptr;
  
  printf("in network_handler\n");
  l = set_interrupt_level(DISABLED);

  if (pkt->size < 0){
    set_interrupt_level(l);
    return;
  }

/*
 *
struct routing_header {
	char routing_packet_type;		// the type of routing packet
	char destination[8];			/ ultimate destination of routing packet
	char id[4];						//identifier value for this broadcast (only applicable for discovery and route reply msgs, 0 otherwise
	char ttl[4];					// number of hops until packet is destroyed (time to live)

	char path_len[4];				// length of route, indicates the number of valid entries in the path array.
									   This should be smaller than MAX_ROUTE_LENGTH
	char path[MAX_ROUTE_LENGTH][8];	// contains the packed network addresses of each node in the route.
									   The address of the source is stored in the first position, and the
									   address of the destination is stored in the last position.
};
 */

  //first check if router packet is destined for us
  if ( (pkt->buffer)[1] == ROUTING_DATA && network_compare_network_addresses(pkt->sender, my_addr) ){
  //if (miniroute_process_packet(pkt){
    //send reply packet
    buf_ptr = pkt->buffer;
    router_hdr = (struct routing_header*)buf_ptr; //set header to data
    ///router_hdr->routing_packet_type = ROUTING_REPLY;
    //offset to path field. 1 + 8 + 4 + 4 + 4 = 21
    memcpy(router_hdr->destination, buf_ptr + 21, 8);
    //keep the same broadcast id
    //router->ttl

   
 
    //pass packet on to tcp/udp 
    //shift contents of pkt buf to overwrite router headers
    shift_by_n(buf_ptr, sizeof(struct routing_header ), (pkt->size - sizeof(struct routing_header)) );
    (pkt->size) -= sizeof(struct routing_header);
    
    
    pkt_hdr = (mini_header_t)(&pkt->buffer);
    protocol = pkt_hdr->protocol;
   
    if (protocol == PROTOCOL_MINIDATAGRAM) {
      if (queue_append(pkt_q, pkt)){
        //queue was not initialized
        set_interrupt_level(l);
        return;
      }
      set_interrupt_level(l);
      semaphore_V(pkt_available_sem); //wake up packet processor
      return;
    }
    else if (protocol == PROTOCOL_MINISTREAM) {
      printf("got a tcp packet\n");
      minisocket_process_packet((void*)pkt);
      set_interrupt_level(l);
    }
    else {
      free(pkt);
      set_interrupt_level(l);
    }   
  }
  else {

  }
}

void
wake_up(void* sem){
  semaphore_V((semaphore_t)sem);
}

/*
 * sleep with timeout in milliseconds
 */
void 
minithread_sleep_with_timeout(int delay){
  semaphore_t thread_sem;
  int num_cycles;

  num_cycles = delay % (TIME_QUANTA/MILLISECOND) == 0? delay/(TIME_QUANTA/MILLISECOND) : delay / (TIME_QUANTA/MILLISECOND) + 1;

  thread_sem = semaphore_create();
  set_alarm(num_cycles, wake_up, (void*)thread_sem, sys_time);
  semaphore_P(thread_sem);
  semaphore_destroy(thread_sem);
}

/* Initialization.
 *
 *      minithread_system_initialize:
 *       This procedure should be called from your C main procedure
 *       to turn a single threaded UNIX process into a multithreaded
 *       program.
 *
 *       Initialize any private data structures.
 *       Create the idle thread.
 *       Fork the thread which should call mainproc(mainarg)
 *       Start scheduling.
 *
 *       Note that the runnable_q is protected by disabling interrupts.
 *       All other data structures are protected with binary semaphores.
 *
 */
void
minithread_system_initialize(proc_t mainproc, arg_t mainarg) {
  minithread_t clean_up_thread = NULL;
  minithread_t process_packets_thread = NULL;
  int a = 0;
  void* dummy_ptr = NULL;
  dummy_ptr = (void*)&a;
  current_id = 0; // the next thread id to be assigned
  network_get_my_address(my_addr);
  id_lock = semaphore_create();
  semaphore_initialize(id_lock,1); 
  runnable_q = multilevel_queue_new(4);
  blocked_q = queue_new();
  blocked_q_lock = semaphore_create();
  semaphore_initialize(blocked_q_lock,1);
  dead_q = queue_new();
  dead_q_lock = semaphore_create();
  semaphore_initialize(dead_q_lock,1);
  dead_sem = semaphore_create();
  semaphore_initialize(dead_sem,0);    
  clean_up_thread = minithread_create(clean_up, NULL);
  multilevel_queue_enqueue(runnable_q,
    clean_up_thread->priority,clean_up_thread);
  runnable_count++;
  minimsg_initialize();
  minisocket_initialize();
  miniroute_initialize();
  process_packets_thread =  minithread_create(process_packets, NULL);
  multilevel_queue_enqueue(runnable_q,
    process_packets_thread->priority,process_packets_thread);
  runnable_count++;
  minithread_clock_init(TIME_QUANTA, (interrupt_handler_t)clock_handler);
  network_initialize((network_handler_t) network_handler);
  init_alarm();
  current_thread = minithread_create(mainproc, mainarg);
  minithread_switch(&dummy_ptr, &(current_thread->stacktop));
  return;
}


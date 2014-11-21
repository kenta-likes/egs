#include "miniroute.h"
#include "miniroute_cache.h"
#include "synch.h"
#include "interrupts.h"
#include "alarm.h"
#include "minithread.h"
#include "miniheader.h"

/* TYPE DEFS
 */
typedef struct resend_arg* resend_arg_t;

/* Note: count, resend_alarm, alarm_arg are accessed with interrupts disabled.
 * the sems accessed with interrupts enabled.
 */
typedef struct discover_control_block {
  int count; //how many threads request this ip addr
  semaphore_t mutex;
  semaphore_t route_ready;
  alarm_t resend_alarm;
  resend_arg_t alarm_arg;
}* dcb_t;
  
struct resend_arg {
  int try_count;
  struct routing_header* hdr;
  dcb_t control_block;
};

void miniroute_resend(void*);

/* GLOBAL VARS
 */
miniroute_cache_t route_cache;        //our route cache
hash_table_t dcb_table;
unsigned int curr_discovery_pkt_id;
network_address_t my_addr;            // my address

/* FUNC DEFS
 */
void miniroute_initialize()
{
  route_cache = miniroute_cache_create();
  dcb_table = hash_table_create();
  curr_discovery_pkt_id = 1;
  network_get_my_address(my_addr); //init my_addr
  return;
}

/* Note: alarm function, so called with interrupts disabled.
 */
void miniroute_resend(void* arg) {
  char tmp;

  resend_arg_t params = (resend_arg_t)arg;
  params->try_count++;
  if (params->try_count >= 3) {
    params->control_block->resend_alarm = NULL;
    params->control_block->alarm_arg = NULL;
    semaphore_V(params->control_block->route_ready); 
    return; 
  }
  //assign fresh id
  pack_unsigned_int(params->hdr->id, curr_discovery_pkt_id++);
  network_bcast_pkt(sizeof(struct routing_header), (char*)(params->hdr), 0, &tmp);
  params->control_block->resend_alarm = set_alarm(120, miniroute_resend, 
      params->control_block->alarm_arg, minithread_time());  
}

/* Returns the route to dest or NULL on failure.
 */
miniroute_t miniroute_discover_route(network_address_t dest) {
  char tmp;
  struct resend_arg arg;
  interrupt_level_t l;
  miniroute_t path;
  dcb_t control_block;
  struct routing_header hdr;
 
  l = set_interrupt_level(DISABLED);
  path = miniroute_cache_get(route_cache, dest);
  if (path != NULL) {
    set_interrupt_level(l);
    return path;
  }
  if (!hash_table_contains(dcb_table, dest)) {
    control_block = (dcb_t)malloc(sizeof(struct discover_control_block));
    if (!control_block) {
      set_interrupt_level(l);
      return NULL;
    }
    control_block->count = 0;
    control_block->mutex = semaphore_create();
    if (!control_block->mutex) {
      free(control_block);
      set_interrupt_level(l);
      return NULL;
    }
    control_block->route_ready = semaphore_create();
    if (!control_block->route_ready) {
      free(control_block);
      semaphore_destroy(control_block->mutex);
      set_interrupt_level(l);
      return NULL;
    }
    semaphore_initialize(control_block->mutex, 1);
    semaphore_initialize(control_block->route_ready, 0);
    control_block->resend_alarm = NULL;
    control_block->alarm_arg = NULL;
    hash_table_add(dcb_table, dest, control_block);
  }
  control_block = hash_table_get(dcb_table, dest);
  if (!control_block) {
    set_interrupt_level(l);
    return NULL;
  }

  control_block->count++;
  set_interrupt_level(l);
  
  semaphore_P(control_block->mutex);
  path = miniroute_cache_get(route_cache, dest);
  if (path) {
    l = set_interrupt_level(DISABLED);
    control_block->count--;
    semaphore_V(control_block->mutex);
    set_interrupt_level(l);
    return path;
  }
  else {
    hdr.routing_packet_type = ROUTING_ROUTE_DISCOVERY;
    pack_address(hdr.destination, dest);
    l = set_interrupt_level(DISABLED);
    pack_unsigned_int(hdr.id, curr_discovery_pkt_id++);
    pack_unsigned_int(hdr.ttl, MAX_ROUTE_LENGTH);
    pack_unsigned_int(hdr.path_len, 1);
    pack_address(hdr.path[0], my_addr);
    //make arg
    arg.try_count = 0;
    arg.hdr = &hdr;
    arg.control_block = control_block;    
    control_block->alarm_arg = &arg; 

    if (network_bcast_pkt(sizeof(struct routing_header), (char*)(&hdr), 0, &tmp) == -1) {
      //error
      control_block->count--;
      semaphore_V(control_block->mutex);
      set_interrupt_level(l);
      return NULL;
    } 
    control_block->resend_alarm = set_alarm(120, miniroute_resend, 
        control_block->alarm_arg, minithread_time());  
    set_interrupt_level(l);
    semaphore_P(control_block->route_ready); 
    //got a reply pkt or timed out
    path = miniroute_cache_get(route_cache, dest);
    l = set_interrupt_level(DISABLED);
    control_block->count--;
    semaphore_V(control_block->mutex);
    set_interrupt_level(l);
    return path;
  }
}

/* sends a miniroute packet, automatically discovering the path if necessary. 
 * See description in the .h file.
 */
int miniroute_send_pkt(network_address_t dest_address, int hdr_len, char* hdr, int data_len, char* data) {
  interrupt_level_t l;
  miniroute_t path;
  dcb_t control_block;

  path = miniroute_discover_route(dest_address);
  path += 1; //just to make compiler happy
  l = set_interrupt_level(DISABLED);
  control_block = hash_table_get(dcb_table, dest_address);
  if (control_block != NULL && control_block->count == 0) {
    //no threads are blocked on route. cleanup
    semaphore_destroy(control_block->mutex); 
    semaphore_destroy(control_block->route_ready);
    hash_table_remove(dcb_table, dest_address);
    free(control_block);
  }
  set_interrupt_level(l);
  return 0;
}



/* hashes a network_address_t into a 16 bit unsigned int */
unsigned short hash_address(network_address_t address)
{
	unsigned int result = 0;
	int counter;

	for (counter = 0; counter < 3; counter++)
		result ^= ((unsigned short*)address)[counter];

	return result % 65521;
}


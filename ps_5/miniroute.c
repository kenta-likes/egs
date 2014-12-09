#include "miniroute.h"
#include "miniroute_cache.h"
#include "synch.h"
#include "interrupts.h"
#include "alarm.h"
#include "minithread.h"
#include "miniheader.h"
#include "network.h"

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
void miniroute_initialize() {
  route_cache = miniroute_cache_create();
  dcb_table = hash_table_create();
  curr_discovery_pkt_id = 1;
  network_get_my_address(my_addr); //init my_addr
  return;
}

/* Takes in a routing packet and does error checking.
 * Adds it to the cache if this packet was destined for us. 
 * Returns 1 if this packet has data to be passed along,
 * O otherwise.
 */
int miniroute_process_packet(network_interrupt_arg_t* pkt) {
  struct routing_header* pkt_hdr = NULL;
  network_address_t tmp_addr;
  network_address_t src_addr;
  network_address_t dst_addr;
  network_address_t nxt_addr;
  unsigned int discovery_pkt_id;
  unsigned int pkt_ttl;
  unsigned int path_len;
  miniroute_t path = NULL; 
  miniroute_t new_path = NULL;
  network_address_t* new_route = NULL;
  unsigned int i;
  unsigned int found;
  struct routing_header hdr;
  char tmp;
  dcb_t control_block;
  
  
  printf("entering miniroute_process_packet\n");
  if (pkt == NULL || pkt->size < sizeof(struct routing_header)) {
    //printf("exiting miniroute_process_packet on INVALID PARAMS\n");
    return 0;
  }
  
  pkt_hdr = (struct routing_header*)pkt->buffer;
  unpack_address(pkt_hdr->destination, dst_addr);
  discovery_pkt_id = unpack_unsigned_int(pkt_hdr->id);
  pkt_ttl = unpack_unsigned_int(pkt_hdr->ttl);
  path_len = unpack_unsigned_int(pkt_hdr->path_len);
  unpack_address(pkt_hdr->path[0], src_addr);

  if (network_compare_network_addresses(my_addr, dst_addr)) {
    //same
    if (!miniroute_cache_get(route_cache, src_addr)) {
      //not in cache 
      if (pkt_hdr->routing_packet_type == ROUTING_ROUTE_DISCOVERY) {
        //add myself to the path vector
        pack_address(pkt_hdr->path[path_len], my_addr); 
        path_len++;
        pack_unsigned_int(pkt_hdr->path_len, path_len);
      }
      new_route = (network_address_t*)calloc(path_len, sizeof(network_address_t));
      if (new_route == NULL) {
        free(pkt);
        //printf("exiting miniroute_process_packet on CALLOC ERROR\n");
        return 0;
      }
      for (i = 0; i < path_len; i++) {
        unpack_address(pkt_hdr->path[path_len - i - 1], tmp_addr);
        network_address_copy(tmp_addr, new_route[i]);
      }
      new_path = (miniroute_t)calloc(1, sizeof(struct miniroute));
      if (new_path == NULL) {
        free(pkt);
        free(new_route);
        //printf("exiting miniroute_process_packet on CALLOC ERROR\n");
        return 0;
      }
      new_path->route = new_route;
      new_path->len = path_len; 
      miniroute_cache_put(route_cache, src_addr, new_path);
    } //added new route to cache
  }
  else if (pkt_ttl <= 0) {
    free(pkt);
    //printf("exiting miniroute_process_packet on TTL ERROR\n");
    return 0;
  }
  else if (pkt_hdr->routing_packet_type != ROUTING_ROUTE_DISCOVERY) {
    //different

    //check from 2nd to second to last address
    found = 0;
    for (i = 1; i < path_len - 1; i++) {
      unpack_address(pkt_hdr->path[i], tmp_addr);
      if (network_compare_network_addresses(my_addr, tmp_addr)) {
        unpack_address(pkt_hdr->path[i+1], nxt_addr);
        found = 1;
        break; 
      }
    }
    if (!found) {
      free(pkt);
      return 0;
    }
  }

  switch (pkt_hdr->routing_packet_type) {
  case ROUTING_DATA:
    //printf("got a DATA pkt\n");
    if (network_compare_network_addresses(my_addr, dst_addr)) {
      //same
      //printf("exiting miniroute_process_packet on DATA PKT\n"); 
      return 1;
    }
    else {
      //skip packet type, shouldn't change
      //skip destination, shouldn't change
      //skip id, shouldn't change
      pack_unsigned_int(pkt_hdr->ttl, pkt_ttl - 1); //subtract ttl
      network_send_pkt(nxt_addr, sizeof(struct routing_header), (char*)pkt_hdr, 0, &tmp);
    }
    break;

  case ROUTING_ROUTE_DISCOVERY:
    if (network_compare_network_addresses(my_addr, dst_addr)) {
      printf("got a DISCOVERY pkt, for me\n");
      //same  
      path = miniroute_cache_get(route_cache, src_addr);

      hdr.routing_packet_type = ROUTING_ROUTE_REPLY;
      pack_address(hdr.destination, src_addr);
      pack_unsigned_int(hdr.id, discovery_pkt_id);
      pack_unsigned_int(hdr.ttl, MAX_ROUTE_LENGTH);
      pack_unsigned_int(hdr.path_len, path->len);
      for (i = 0; i < path->len; i++) {
        pack_address(hdr.path[i], path->route[i]);
      }
      network_send_pkt(path->route[1], sizeof(struct routing_header), (char*)(&hdr), 0, &tmp);
    }
    else {
      //printf("got a DISCOVERY pkt, for someone else\n");
      //different
      //scan to check if i am in list
      //if yes then discard
      //else append to path vector and broadcast
      //
      //scan to check if i am in list
      //if yes then pass along, else discard
      for (i = 0; i < path_len - 1; i++) {
        unpack_address(pkt_hdr->path[i], tmp_addr);
        if (network_compare_network_addresses(my_addr, tmp_addr)) {
          free(pkt);
         // printf("exiting miniroute_process_packet on BROADCAST LOOP\n");
          return 0;
        }
      }
      //printf("checks passed\n");
      pack_address(pkt_hdr->path[path_len], my_addr);
      pack_unsigned_int(pkt_hdr->path_len, path_len + 1); //add path_len
      pack_unsigned_int(pkt_hdr->ttl, pkt_ttl - 1); //subtract ttl
      //printf("packet header configured\n");
      //printf("my addr is (%i,%i)\n", my_addr[0], my_addr[1]);
      //printf("source addr is (%i,%i)\n", src_addr[0], src_addr[1]);
      //printf("dst addr is (%i,%i)\n", dst_addr[0], dst_addr[1]);
      //for (i = 0 ; i < path_len + 1; i++){
        //unpack_address(pkt_hdr->path[i], tmp_addr);
        //printf("->(%i,%i)", tmp_addr[0], tmp_addr[1]);
      //}
      //printf("\n");
      network_bcast_pkt(sizeof(struct routing_header), (char*)pkt_hdr, 0, &tmp); //send to neighbors
      //printf("broadcast successful\n");
    }
    break;

  case ROUTING_ROUTE_REPLY:
    printf("got a REPLY pkt\n");
    if (network_compare_network_addresses(my_addr, dst_addr)) {
      //same
      control_block = hash_table_get(dcb_table, src_addr);
      if (control_block) {
        deregister_alarm(control_block->resend_alarm);
        control_block->resend_alarm = NULL;
        control_block->alarm_arg = NULL;
        semaphore_V(control_block->route_ready);
      }
    }
    else {
      //different
      //check ttl
      //scan to check if i am in list
      //if yes then pass along, else discard
      //
      //skip packet type, shouldn't change
      //skip destination, shouldn't change
      //skip id, shouldn't change
      pack_unsigned_int(pkt_hdr->ttl, pkt_ttl - 1); //subtract ttl
      network_send_pkt(nxt_addr, sizeof(struct routing_header), (char*)pkt_hdr, 0, &tmp);
    }
    break;

  default:
    //WTFFF???
    break;
  }    
  //printf("exiting miniroute_process_packet on SUCCESS\n"); 
  free(pkt);
  return 0;
}

/* Note: alarm function, so called with interrupts disabled.
 */
void miniroute_resend(void* arg) {
  char tmp;
  resend_arg_t params = (resend_arg_t)arg;

  //printf("entering miniroute_resend\n");
  params->try_count++;
  if (params->try_count >= 3) {
    printf("timed out when trying to reach host\n");
    params->control_block->resend_alarm = NULL;
    params->control_block->alarm_arg = NULL;
    semaphore_V(params->control_block->route_ready); 
    return; 
  }
  //assign fresh id
  pack_unsigned_int(params->hdr->id, curr_discovery_pkt_id++);
  //printf("sending another DISCOVERY pkt\n");
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

  //printf("entering miniroute_discover_route\n"); 
  l = set_interrupt_level(DISABLED);
  path = miniroute_cache_get(route_cache, dest);
  if (path != NULL) {
    //printf("got route from cache\n");
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
    //printf("made a NEW discover control block\n"); 
    }
  control_block = hash_table_get(dcb_table, dest);
  if (!control_block) {
    //printf("ERROR: could not find discover control block\n");
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
    //printf("exiting miniroute_discover_route on SUCCESS\n"); 
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

    //printf("sending first DISCOVERY pkt\n");
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
    //printf("exiting miniroute_discover_route on SUCCESS\n"); 
    return path;
  }
}

/* sends a miniroute packet, automatically discovering the path if necessary. 
 * See description in the .h file.
 */
int miniroute_send_pkt(network_address_t dest_address, int hdr_len, 
                        char* hdr, int data_len, char* data) {
  interrupt_level_t l;
  miniroute_t path;
  dcb_t control_block;
  struct routing_header new_hdr;
  int i;
  int path_len;
  int new_data_len;
  char* new_data;
  int bytes_sent;
  
  //printf("entering miniroute_send_pkt\n");
  if (hdr_len < 0 || data_len < 0 || hdr == NULL || data == NULL) {
    //printf("invalid params\n");
    return -1;
  }
  path = miniroute_discover_route(dest_address);

  if (path == NULL) {
    return -1;
  }

  l = set_interrupt_level(DISABLED);
  control_block = hash_table_get(dcb_table, dest_address);
  if (control_block != NULL && control_block->count == 0) {
    //no threads are blocked on route. cleanup
    semaphore_destroy(control_block->mutex); 
    semaphore_destroy(control_block->route_ready);
    hash_table_remove(dcb_table, dest_address);
    free(control_block);
    //printf("destroyed a discovery control block\n");
  }
  set_interrupt_level(l);

  //JUST DO IT
  new_hdr.routing_packet_type = ROUTING_DATA;
  pack_address(new_hdr.destination, dest_address);
  pack_unsigned_int(new_hdr.ttl, MAX_ROUTE_LENGTH);
  pack_unsigned_int(new_hdr.id, 0);
  pack_unsigned_int(new_hdr.path_len, path->len);
  if (path->len > MAX_ROUTE_LENGTH) {
    path_len = MAX_ROUTE_LENGTH; 
  }
  else {
    path_len = path->len;
  }
  for (i = 0; i < path_len; i++) {
    pack_address(new_hdr.path[i], path->route[i]);
  }
  new_data_len = hdr_len + data_len;
  new_data = (char*)malloc(new_data_len);
  memcpy(new_data, hdr, hdr_len);
  memcpy(new_data+hdr_len, data, data_len);
  bytes_sent = network_send_pkt(path->route[1], sizeof(struct routing_header), 
      (char*)&new_hdr, new_data_len, new_data);
  free(new_data);
  if ((bytes_sent - hdr_len) < 0) {
    //printf("exiting miniroute_send_pkt on FAILURE\n");
    return -1;
  }
  else {
    //printf("exiting miniroute_send_pkt on SUCCESS\n");
    return (bytes_sent - hdr_len);
  }
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


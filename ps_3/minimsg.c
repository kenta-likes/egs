/*
 *  Implementation of minimsgs and miniports.
 */
#include "minimsg.h"
#include "interrupts.h"
#include "miniheader.h"
#include <stdlib.h>

#define MAX_PORT_NUM 65536
#define UNBOUND_PORT_START 0 
#define BOUND_PORT_START 32768

enum port_type { UNBOUND_PORT = 1, BOUND_PORT};

union port_union{
  struct unbound
  {
    queue_t port_pkt_q; //queue for packets
    semaphore_t port_pkt_available_sem; //counting semaphore for thread blocking
    semaphore_t q_lock; //binary semaphore for mutual exclusion
  } unbound;
  struct bound
  {
    unsigned short dest_num; //the remote unbound port number this sends to
    network_address_t dest_addr; //the remote address
  } bound;
};

struct miniport{
  enum port_type p_type;
  unsigned short p_num;
  union port_union u;
};

/**
 *  Global variables
 */

network_address_t my_addr;      // my address

miniport_t* miniport_array;     // this array contains pointers to miniports
                                // where the index corresponds to the port number
                                // null if the port at that index has not been created
semaphore_t bound_ports_lock;   // this lock ensures mutual exclusion for creating
                                // or destroying bound ports
semaphore_t unbound_ports_lock; // this lock ensures mutual exclusion for creating
                                // or destroying unbound ports

queue_t pkt_q;                  // buffer for holding recieved packets for the system
                                // process_packets_thread takes from this queue
                                // and puts the processed packet into the intended unbound port
                                // protected by disabling interrupts
semaphore_t pkt_available_sem;  // counting sem for the number of packets available

unsigned short curr_bound_index;


/* performs any required initialization of the minimsg layer.
 */
void
minimsg_initialize() {
  unsigned int i;
  
  network_get_my_address(my_addr); //init my_addr
  curr_bound_index = BOUND_PORT_START;
  miniport_array = (miniport_t*)malloc((MAX_PORT_NUM)*sizeof(miniport_t));
  if (miniport_array == NULL) {
    return;
  }
  for (i = 0; i < MAX_PORT_NUM; i++) {
    miniport_array[i] = NULL;
  }
 /* 
  bound_ports_lock = semaphore_create();
  semaphore_initialize(bound_ports_lock,1);
  unbound_ports_lock = semaphore_create();
  semaphore_initialize(unbound_ports_lock,1);
  
  pkt_q = queue_new();
  pkt_available_sem = semaphore_create();
  semaphore_initialize(pkt_available_sem,0); */ 
}


/**
 * method for packet processor to repeatedly
 * check for new packets that arrived and upon
 * sanity checks forward them to the appropriate
 * port to be queued up. If the destination port
 * is not initialized, the packet is thrown away.
 *
 * We protect important global data structures (ie.
 * our array of miniports) with semaphores. Each 
 * miniport has an associated semaphore as well.
 */
int process_packets() {
  interrupt_level_t l;
  network_interrupt_arg_t* pkt;
  char protocol;
  network_address_t src_addr;
  mini_header_t header; 
  unsigned short src_port_num;
  network_address_t dst_addr;
  unsigned short dst_port_num;
  //char message_type;
  //unsigned int seq_number;
  //unsigned int ack_number; // TCP
  miniport_t dst_port;

  while (1) {
    semaphore_P(pkt_available_sem);
    printf("in process packets!!!!\n");
    l = set_interrupt_level(DISABLED);
    if (queue_dequeue(pkt_q, (void**)&pkt)){
      //dequeue fails
      set_interrupt_level(l);
      continue; //move on with life
    }
    set_interrupt_level(l);

    //perform checks on packet, free & return if invalid
    protocol = pkt->buffer[0];
    if (protocol != PROTOCOL_MINIDATAGRAM &&
          protocol != PROTOCOL_MINISTREAM){
      free(pkt);
      continue;
    } 
    else {
      // JUMP ON IT
      header = (mini_header_t)(&pkt->buffer);
      unpack_address(header->source_address, src_addr);
      src_port_num = unpack_unsigned_short(header->source_port);
      unpack_address(header->destination_address, dst_addr);
      dst_port_num = unpack_unsigned_short(header->destination_port);

      if (protocol == PROTOCOL_MINIDATAGRAM){
        //check address
        if (!network_compare_network_addresses(my_addr, dst_addr) ||
              src_port_num >= BOUND_PORT_START ||
              dst_port_num >= BOUND_PORT_START ) {
          free(pkt);
          continue;
        }
        //if port DNE or not an unbound port, fail
        if (miniport_array[dst_port_num] == NULL ||
            miniport_array[dst_port_num]->p_type != UNBOUND_PORT) {
          free(pkt);
          continue;
        }
        dst_port = miniport_array[dst_port_num]; 
        semaphore_P(dst_port->u.unbound.q_lock);
        queue_append(dst_port->u.unbound.port_pkt_q,pkt);
        semaphore_V(dst_port->u.unbound.q_lock);
        semaphore_V(dst_port->u.unbound.port_pkt_available_sem);
        continue;
      }
      else if (protocol == PROTOCOL_MINISTREAM) {
        free(pkt);
        continue; //for now, ignore tcp packets
      }
    }
    //DROP DA BASE
    //JUUUUUMPP ONNNN ITTT
  }
  return -1;
}

/* Creates an unbound port for listening. Multiple requests to create the same
 * unbound port should return the same miniport reference. It is the responsibility
 * of the programmer to make sure he does not destroy unbound miniports while they
 * are still in use by other threads -- this would result in undefined behavior.
 * Unbound ports must range from 0 to 32767. If the programmer specifies a port number
 * outside this range, it is considered an error.
 */
miniport_t
miniport_create_unbound(int port_number) {
  miniport_t new_port;

  if (port_number < 0 || port_number >= BOUND_PORT_START) {
    // user error
    return NULL;
  }
  if (miniport_array[port_number]) {
    return miniport_array[port_number];
  }
  semaphore_P(unbound_ports_lock);
  new_port = (miniport_t)malloc(sizeof(struct miniport)); 
  if (new_port == NULL) {
    semaphore_V(unbound_ports_lock);
    return NULL;
  }
  new_port->p_type = UNBOUND_PORT;
  new_port->p_num = port_number;
  new_port->u.unbound.port_pkt_q = queue_new();
  if (!new_port->u.unbound.port_pkt_q) {
    free(new_port);
    semaphore_V(unbound_ports_lock);
    return NULL;
  }
  new_port->u.unbound.port_pkt_available_sem = semaphore_create();
  if (!new_port->u.unbound.port_pkt_available_sem) {
    queue_free(new_port->u.unbound.port_pkt_q);
    free(new_port);
    semaphore_V(unbound_ports_lock);
    return NULL;
  } 
  new_port->u.unbound.q_lock = semaphore_create();
  if (!new_port->u.unbound.q_lock) {
    queue_free(new_port->u.unbound.port_pkt_q);
    semaphore_destroy(new_port->u.unbound.port_pkt_available_sem);
    free(new_port);
    semaphore_V(unbound_ports_lock);
    return NULL;
  }
  semaphore_initialize(new_port->u.unbound.port_pkt_available_sem,0);
  semaphore_initialize(new_port->u.unbound.q_lock,1);
  miniport_array[port_number] = new_port;
  semaphore_V(unbound_ports_lock);
  return new_port;
   
}

/* Creates a bound port for use in sending packets. The two parameters, addr and
 * remote_unbound_port_number together specify the remote's listening endpoint.
 * This function should assign bound port numbers incrementally between the range
 * 32768 to 65535. Port numbers should not be reused even if they have been destroyed,
 * unless an overflow occurs (ie. going over the 65535 limit) in which case you should
 * wrap around to 32768 again, incrementally assigning port numbers that are not
 * currently in use.
 */
miniport_t
miniport_create_bound(network_address_t addr, int remote_unbound_port_number)
{
  unsigned short start;
  miniport_t new_port;

  semaphore_P(bound_ports_lock);
  start = curr_bound_index;
  while (miniport_array[curr_bound_index] != NULL){
    curr_bound_index += 1;
    if (curr_bound_index >= MAX_PORT_NUM){
      curr_bound_index = BOUND_PORT_START;
    }
    if (curr_bound_index == start){ //bound port array full
      semaphore_V(bound_ports_lock);
      return NULL;
    }
  }

  new_port = (miniport_t)malloc(sizeof(struct miniport)); 
  if (new_port == NULL) {
    semaphore_V(bound_ports_lock);
    return NULL;
  }
  new_port->p_type = BOUND_PORT;
  new_port->p_num = curr_bound_index;
  new_port->u.bound.dest_num = remote_unbound_port_number;
  network_address_copy(addr, new_port->u.bound.dest_addr);
  miniport_array[curr_bound_index] = new_port;
  curr_bound_index += 1; //point to next slot
  if (curr_bound_index >= MAX_PORT_NUM){
    curr_bound_index = BOUND_PORT_START;
  }
  semaphore_V(bound_ports_lock);
  return new_port;
}

/* Destroys a miniport and frees up its resources. If the miniport was in use at
 * the time it was destroyed, subsequent behavior is undefined.
 */
void
miniport_destroy(miniport_t miniport)
{
  if (miniport == NULL
      || miniport->p_num >= MAX_PORT_NUM
      || (miniport->p_type != UNBOUND_PORT && miniport->p_type != BOUND_PORT )){
    return;
  }
  if (miniport->p_type == UNBOUND_PORT) {
    semaphore_P(unbound_ports_lock);
    miniport_array[miniport->p_num] = NULL;
    queue_free(miniport->u.unbound.port_pkt_q);
    semaphore_destroy(miniport->u.unbound.port_pkt_available_sem);
    semaphore_destroy(miniport->u.unbound.q_lock);
    free(miniport);
    semaphore_V(unbound_ports_lock);
  } 
  else {
    semaphore_P(bound_ports_lock);
    miniport_array[miniport->p_num] = NULL;
    free(miniport);
    semaphore_V(bound_ports_lock);
  }
}

/* Sends a message through a locally bound port (the bound port already has an associated
 * receiver address so it is sufficient to just supply the bound port number). In order
 * for the remote system to correctly create a bound port for replies back to the sending
 * system, it needs to know the sender's listening port (specified by local_unbound_port).
 * The msg parameter is a pointer to a data payload that the user wishes to send and does not
 * include a network header; your implementation of minimsg_send must construct the header
 * before calling network_send_pkt(). The return value of this function is the number of
 * data payload bytes sent not inclusive of the header. Returns -1 on error.
 * Fails if msg is too long. 
 */
int
minimsg_send(miniport_t local_unbound_port, miniport_t local_bound_port, minimsg_t msg, int len) {
  struct mini_header hdr;
  network_address_t dst_addr;
  
  if (len > MINIMSG_MAX_MSG_SIZE) {
    return -1;
  }

  if (local_unbound_port == NULL || 
      local_unbound_port->p_type != UNBOUND_PORT || 
      local_unbound_port->p_num >= BOUND_PORT_START ||
      miniport_array[local_unbound_port->p_num] != local_unbound_port) {
    return -1;
  }

  if (local_bound_port == NULL ||
      local_bound_port->p_type != BOUND_PORT ||
      local_bound_port->p_num < BOUND_PORT_START ||
      miniport_array[local_bound_port->p_num] != local_bound_port) {
    return -1;
  }

  network_address_copy(local_bound_port->u.bound.dest_addr, dst_addr); 
  hdr.protocol = PROTOCOL_MINIDATAGRAM;
  pack_address(hdr.source_address, my_addr);
  pack_unsigned_short(hdr.source_port, local_unbound_port->p_num);
  pack_address(hdr.destination_address, local_bound_port->u.bound.dest_addr);
  pack_unsigned_short(hdr.destination_port, local_bound_port->u.bound.dest_num);
  
  if (network_send_pkt(dst_addr, sizeof(hdr), (char*)&hdr, len, msg)) {
    return -1;
  }
  return len;
}

/* Receives a message through a locally unbound port. Threads that call this function are
 * blocked until a message arrives. Upon arrival of each message, the function must create
 * a new bound port that targets the sender's address and listening port, so that use of
 * this created bound port results in replying directly back to the sender. It is the
 * responsibility of this function to strip off and parse the header before returning the
 * data payload and data length via the respective msg and len parameter. The return value
 * of this function is the number of data payload bytes received not inclusive of the header.
 */
int minimsg_receive(miniport_t local_unbound_port, miniport_t* new_local_bound_port, minimsg_t msg, int *len)
{
  network_interrupt_arg_t* pkt;
  mini_header_t pkt_header;
  char protocol;
  network_address_t src_addr;
  unsigned short src_port;
  network_address_t dst_addr;
  int i;
  char* buff;
  if (local_unbound_port == NULL
        || local_unbound_port->p_type != UNBOUND_PORT
        || local_unbound_port->p_num >= BOUND_PORT_START
        || miniport_array[local_unbound_port->p_num] != local_unbound_port){
    return -1;
  }
  //block until packet arrives
  semaphore_P(local_unbound_port->u.unbound.port_pkt_available_sem);

  semaphore_P(local_unbound_port->u.unbound.q_lock);
  if( queue_dequeue(local_unbound_port->u.unbound.port_pkt_q,
                  (void**)&pkt)){
    return -1;
  }
  semaphore_V(local_unbound_port->u.unbound.q_lock);

  pkt_header = (mini_header_t)(&pkt->buffer);
  protocol = pkt_header->protocol;
  unpack_address(pkt_header->source_address, src_addr);
  src_port = unpack_unsigned_short(pkt_header->source_port);
  unpack_address(pkt_header->destination_address, dst_addr);
  if (protocol != PROTOCOL_MINIDATAGRAM
        && protocol != PROTOCOL_MINISTREAM){
    return -1;
  }
  if (protocol == PROTOCOL_MINISTREAM){
    return 0; //currently unsupported
  } 
  else { //UDP
    if ( pkt->size < sizeof(struct mini_header)){
      return -1;
    }
    *len = pkt->size-sizeof(struct mini_header) > *len?
                  *len : pkt->size-sizeof(struct mini_header);
    *new_local_bound_port = miniport_create_bound(pkt->sender, src_port);
    //copy payload
    buff = (char*)&(pkt->buffer);
    buff += sizeof(struct mini_header);
    for (i = 0; i < *len; i++){
      msg[i] = *buff;
      buff++;
    }
    free(pkt); 
    return *len;
  }
}


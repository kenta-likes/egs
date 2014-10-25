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
  struct miniport_unbound
  {
    unsigned short port_num;//the local port number
    queue_t packet_q;//queue for packets
    semaphore_t port_sem;//counting semaphore for thread blocking
    semaphore_t port_lock;//binary semaphore for mutual exclusion
  } miniport_unbound;
  struct miniport_bound
  {
    unsigned short port_num;//the local port number
    unsigned short dest_num;//the remote unbound port number this sends to
    semaphore_t port_lock;//binary semaphore for mutual exclusion
  } miniport_bound;
};

struct miniport{
  enum port_type p_type;
  union port_union m_port;
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


/* performs any required initialization of the minimsg layer.
 */
void
minimsg_initialize() {
  unsigned int i;
  
  network_get_my_address(my_addr); //init my_addr
  miniport_array = (miniport_t*)malloc((MAX_PORT_NUM)*sizeof(miniport_t));
  for (i = 0; i < MAX_PORT_NUM; i++) {
    miniport_array[i] = 0;
  }
  
  bound_ports_lock = semaphore_create();
  semaphore_initialize(bound_ports_lock,1);
  unbound_ports_lock = semaphore_create();
  semaphore_initialize(unbound_ports_lock,1);
  
  pkt_q = queue_new();
  pkt_available_sem = semaphore_create();
  semaphore_initialize(pkt_available_sem,0);  
}


/**
 * method for packet processor to repeatedly
 * check for new packets that arrived and upon
 * sanity checks forward them to the appropriate
 * port to be queued up. If the destination port
 * is not initialized, the packet is thrown away.
 */
void process_packets(){
  interrupt_level_t l;
  network_interrupt_arg_t* pkt;
  char protocol;
  network_address_t source_address;
  unsigned short source_port;
  network_address_t destination_address;
  unsigned short destination_port;
  //char message_type;
  //unsigned int seq_number;
  //unsigned int ack_number; // TCP
  char* buff;

  while (1) {
    semaphore_P(pkt_available_sem);
    l = set_interrupt_level(DISABLED);
    if (queue_dequeue(pkt_q, (void**)&pkt)){
      //dequeue fails
      set_interrupt_level(l);
      continue;//move on with life
    }
    set_interrupt_level(l);

    //perform checks on packet, free & return if invalid
    protocol = (char)unpack_unsigned_short(pkt->buffer);
    if (protocol != PROTOCOL_MINIDATAGRAM &&
          protocol != PROTOCOL_MINISTREAM){
      free(pkt);
      continue;
    } else {
      //JUMP ON IT
      buff = (char*)(&pkt->buffer);
      buff++;
      unpack_address(buff, source_address);
      buff += 8;
      source_port = unpack_unsigned_short(buff);
      buff += 2;
      unpack_address(buff, destination_address);
      buff += 8;
      destination_port = unpack_unsigned_short(buff);
      buff += 2;
      if (protocol == PROTOCOL_MINIDATAGRAM){
        //check address
        if (!network_compare_network_addresses(my_addr, destination_address) ||
              source_port >= BOUND_PORT_START ||
              destination_port >= BOUND_PORT_START ){
          free(pkt);
          continue;
        }
        //overwrite buffer
        //if port DNE, fail
        //enqueue network_interrupt_arg_t to appropriate port queue
        //continue
      }
      else if (protocol == PROTOCOL_MINISTREAM){
        free(pkt);
        continue; //for now, ignore tcp packets
      }
    }
    
    //DROP DA BASE
    //JUUUUUMPP ONNNN ITTT
  }

}

/* Creates an unbound port for listening. Multiple requests to create the same
 * unbound port should return the same miniport reference. It is the responsibility
 * of the programmer to make sure he does not destroy unbound miniports while they
 * are still in use by other threads -- this would result in undefined behavior.
 * Unbound ports must range from 0 to 32767. If the programmer specifies a port number
 * outside this range, it is considered an error.
 */
miniport_t
miniport_create_unbound(int port_number)
{
    return 0;
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
    return 0;
}

/* Destroys a miniport and frees up its resources. If the miniport was in use at
 * the time it was destroyed, subsequent behavior is undefined.
 */
void
miniport_destroy(miniport_t miniport)
{
}

/* Sends a message through a locally bound port (the bound port already has an associated
 * receiver address so it is sufficient to just supply the bound port number). In order
 * for the remote system to correctly create a bound port for replies back to the sending
 * system, it needs to know the sender's listening port (specified by local_unbound_port).
 * The msg parameter is a pointer to a data payload that the user wishes to send and does not
 * include a network header; your implementation of minimsg_send must construct the header
 * before calling network_send_pkt(). The return value of this function is the number of
 * data payload bytes sent not inclusive of the header.
 */
int
minimsg_send(miniport_t local_unbound_port, miniport_t local_bound_port, minimsg_t msg, int len)
{
    return 0;
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
    return 0;
}


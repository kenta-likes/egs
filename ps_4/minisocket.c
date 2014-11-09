/*
 *	Implementation of minisockets.
 */
#include "minisocket.h"
#include <stdlib.h>
#include <stdio.h>
#include "synch.h"
#include "queue.h"
#include "alarm.h"
#include "miniheader.h"
#include "interrupts.h"

#define NUM_SOCKETS 65536
#define SERVER_START 0
#define CLIENT_START 32768

typedef enum state { LISTEN = 1, CONNECTING, CONNECTED, WAIT, EXIT} state;

struct minisocket
{
  state curr_state;
  int try_count;
  unsigned int curr_ack;
  unsigned int curr_seq;
  alarm_t resend_alarm; 
  semaphore_t pkt_ready_sem; 
  queue_t pkt_q;
  unsigned short src_port;
  network_address_t dst_addr;
  unsigned short dst_port;
};

minisocket_t* sock_array;
semaphore_t client_lock;
semaphore_t server_lock;
network_address_t my_addr;

/* Initializes the minisocket layer. */
void minisocket_initialize()
{
  sock_array = (minisocket_t*)malloc(sizeof(struct minisocket)*NUM_SOCKETS);
  if (!sock_array) {
    return;
  }
  client_lock = semaphore_create();
  if (!client_lock) {
    free(sock_array);
    return;
  }
  server_lock = semaphore_create();
  if (!server_lock) {
    free(sock_array);
    semaphore_destroy(client_lock);
    return;
  }
  semaphore_initialize(client_lock, 1);
  semaphore_initialize(server_lock, 1);
  network_get_my_address(my_addr);
}


/* 
 * Listen for a connection from somebody else. When communication link is
 * created return a minisocket_t through which the communication can be made
 * from now on.
 *
 * The argument "port" is the port number on the local machine to which the
 * client will connect.
 *
 * Return value: the minisocket_t created, otherwise NULL with the errorcode
 * stored in the "error" variable.
 */
minisocket_t minisocket_server_create(int port, minisocket_error *error)
{
  minisocket_t new_sock;
  interrupt_level_t l;
  network_interrupt_arg_t * pkt;
  mini_header_reliable_t pkt_hdr;
  char protocol;
  network_address_t src_addr;
  unsigned short src_port;
  network_address_t dst_addr;
  unsigned int seq_num;
  unsigned int ack_num;
  

  //check valid portnum
  if (port < 0 || port >= CLIENT_START){
    *error = SOCKET_INVALIDPARAMS;
    return NULL;
  }
  //check port in use
  semaphore_P(server_lock);
  if (sock_array[port] != NULL){
    *error = SOCKET_PORTINUSE;
    return NULL;
  }
  new_sock = (minisocket_t)malloc(sizeof(struct minisocket));
  if (!new_sock){
    semaphore_V(server_lock);
    *error = SOCKET_OUTOFMEMORY;
    return NULL;
  }
  new_sock->pkt_ready_sem = semaphore_create();
  if (!(new_sock->pkt_ready_sem)){
    semaphore_V(server_lock);
    free(new_sock);
    *error = SOCKET_OUTOFMEMORY;
    return NULL;
  }
  new_sock->pkt_q = queue_new();
  if (!(new_sock->pkt_q)){
    semaphore_V(server_lock);
    free(new_sock->pkt_ready_sem);
    free(new_sock);
    *error = SOCKET_OUTOFMEMORY;
    return NULL;
  }
  semaphore_initialize(new_sock->pkt_ready_sem, 0);

  new_sock->curr_state = LISTEN;
  new_sock->try_count = 0;
  new_sock->curr_ack = 0;
  new_sock->curr_seq = 1;
  new_sock->resend_alarm = NULL;//no alarm set
  new_sock->src_port = port;
  new_sock->dst_port = -1;//not paired with client
  network_address_blankify(new_sock->dst_addr);//not paired with client

  sock_array[port] = new_sock;
  semaphore_V(server_lock);
 
  while (new_sock->curr_state == LISTEN){
    semaphore_P(new_sock->pkt_ready_sem);
    l = set_interrupt_level(DISABLED);
    if (queue_dequeue(new_sock->pkt_q, (void**)&pkt) == -1){
      *error = SOCKET_RECEIVEERROR;
      set_interrupt_level(l);
      continue;
    }
    //check packet size at least sizeof tcp header
    if (pkt->size < sizeof(struct mini_header_reliable)) {
      *error = SOCKET_RECEIVEERROR;
      free(pkt);
      set_interrupt_level(l);
      continue;
    }
    pkt_hdr = (mini_header_reliable_t)(&pkt->buffer);
    protocol = pkt_hdr->protocol;
    if (protocol != PROTOCOL_MINISTREAM){
      *error = SOCKET_RECEIVEERROR;
      free(pkt);
      set_interrupt_level(l);
      continue;
    }
    unpack_address(pkt_hdr->source_address, src_addr);
    src_port = unpack_unsigned_short(pkt_hdr->source_port);
    if (src_port < CLIENT_START || src_port >= NUM_SOCKETS){
      *error = SOCKET_RECEIVEERROR;
      free(pkt);
      set_interrupt_level(l);
      continue;
    }
    unpack_address(pkt_hdr->destination_address, dst_addr);
    if (!network_compare_network_addresses(dst_addr, my_addr)){
      *error = SOCKET_RECEIVEERROR;
      free(pkt);
      set_interrupt_level(l);
      continue;
    }
    //TCP!!!!
    seq_num = unpack_unsigned_int(pkt_hdr->seq_number);
    ack_num = unpack_unsigned_int(pkt_hdr->ack_number);
    if (pkt_hdr->message_type != MSG_SYN || seq_num != 1 || ack_num != 0){
      *error = SOCKET_RECEIVEERROR;
      free(pkt);
      set_interrupt_level(l);
      continue;
    }
    new_sock->curr_state = WAIT;
    new_sock->curr_ack++;
    //create SYNACK packet

    set_interrupt_level(l);
    
    
  }
  return new_sock;
}


/*
 * Initiate the communication with a remote site. When communication is
 * established create a minisocket through which the communication can be made
 * from now on.
 *
 * The first argument is the network address of the remote machine. 
 *
 * The argument "port" is the port number on the remote machine to which the
 * connection is made. The port number of the local machine is one of the free
 * port numbers.
 *
 * Return value: the minisocket_t created, otherwise NULL with the errorcode
 * stored in the "error" variable.
 */
minisocket_t minisocket_client_create(network_address_t addr, int port, minisocket_error *error)
{
  return NULL;
}


/* 
 * Send a message to the other end of the socket.
 *
 * The send call should block until the remote host has ACKnowledged receipt of
 * the message.  This does not necessarily imply that the application has called
 * 'minisocket_receive', only that the packet is buffered pending a future
 * receive.
 *
 * It is expected that the order of calls to 'minisocket_send' implies the order
 * in which the concatenated messages will be received.
 *
 * 'minisocket_send' should block until the whole message is reliably
 * transmitted or an error/timeout occurs
 *
 * Arguments: the socket on which the communication is made (socket), the
 *            message to be transmitted (msg) and its length (len).
 * Return value: returns the number of successfully transmitted bytes. Sets the
 *               error code and returns -1 if an error is encountered.
 */
int minisocket_send(minisocket_t socket, minimsg_t msg, int len, minisocket_error *error)
{
  return -1;
}

/*
 * Receive a message from the other end of the socket. Blocks until
 * some data is received (which can be smaller than max_len bytes).
 *
 * Arguments: the socket on which the communication is made (socket), the memory
 *            location where the received message is returned (msg) and its
 *            maximum length (max_len).
 * Return value: -1 in case of error and sets the error code, the number of
 *           bytes received otherwise
 */
int minisocket_receive(minisocket_t socket, minimsg_t msg, int max_len, minisocket_error *error)
{
  return -1;
}

/* Close a connection. If minisocket_close is issued, any send or receive should
 * fail.  As soon as the other side knows about the close, it should fail any
 * send or receive in progress. The minisocket is destroyed by minisocket_close
 * function.  The function should never fail.
 */
void minisocket_close(minisocket_t socket)
{

}

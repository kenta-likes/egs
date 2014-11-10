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
#include "minithread.h"

#define NUM_SOCKETS 65536
#define SERVER_START 0
#define CLIENT_START 32768
#define RESEND_TIME_UNIT 1

typedef enum state {LISTEN = 1, CONNECTING, CONNECT_WAIT, CONNECTED, WAIT, EXIT} state;

struct minisocket
{
  state curr_state;
  int try_count;
  unsigned int curr_ack;
  unsigned int curr_seq;
  alarm_t resend_alarm; 
  semaphore_t pkt_ready_sem; 
  semaphore_t ack_ready_sem;
  semaphore_t sock_lock; //all mighty sock lock
  queue_t pkt_q;
  unsigned short src_port;
  network_address_t dst_addr;
  unsigned short dst_port;
};

typedef struct resend_arg {
  minisocket_t sock;
  unsigned int data_len;
  char* data;
  char msg_type;
  minisocket_error *error;
}* resend_arg_t;

minisocket_t* sock_array;
semaphore_t client_lock;
semaphore_t server_lock;
network_address_t my_addr;
unsigned int curr_client_idx;


/* minisocket_resend takes in a resend_arg with fields on the socket on which the send,
 * and what to send.
 * This function will be passed in to an alarm to be called later.
 * If the alarm is called, we will increment try_count, resend and set another alarm 
 * unless we have reached 7 tries, in which case we fail.
 * Note: Interrupts are disabled whenever this function is called,
 * since it is passed in as an alarm func.
 */
void minisocket_resend(void* arg);

/* minisocket_send_ctrl creates an ctrl packet of type type and with
 * fields taken from the sock parameter.
 * This ack packet is sent over the network.
 * If there is an underlying network failure, error is updated
 * but the pkt is not resent.
 * The address of pkt is given as the data buffer, 
 * but no data from pkt is written since the data_len is 0.
 */ 
void minisocket_send_ctrl(char type, minisocket_t sock, minisocket_error* error) {
  struct mini_header_reliable pkt;
  pkt.protocol = PROTOCOL_MINISTREAM;
  pack_address(pkt.source_address, my_addr);
  pack_unsigned_short(pkt.source_port, sock->src_port);
  pack_address(pkt.destination_address, sock->dst_addr);
  pack_unsigned_short(pkt.destination_port, sock->dst_port);
  pkt.message_type = type;
  pack_unsigned_int(pkt.seq_number, sock->curr_seq);
  pack_unsigned_int(pkt.ack_number, sock->curr_ack);
  
  if (network_send_pkt(sock->dst_addr, sizeof(pkt), 
      (char*)&pkt, 0, (char*)&pkt) == -1) {
    *error = SOCKET_SENDERROR;
  }  
}

/* minisocket_send_data creates a header pkt with fields taken from sock parameter.
 * The header and the data payload are sent over the network.
 * If there is an underlying network failure, error is updated
 * but the pkt is not resent at this level.
 * Since this function may be called on a resend, seq_number is not automatically updated. 
 * It is the responsibility of minisocket_send to update the seq_number.
 */
void minisocket_send_data(minisocket_t sock, unsigned int data_len, char* data, minisocket_error* error) {
  struct mini_header_reliable pkt;
  pkt.protocol = PROTOCOL_MINISTREAM;
  pack_address(pkt.source_address, my_addr);
  pack_unsigned_short(pkt.source_port, sock->src_port);
  pack_address(pkt.destination_address, sock->dst_addr);
  pack_unsigned_short(pkt.destination_port, sock->dst_port);
  pkt.message_type = MSG_ACK;
  pack_unsigned_int(pkt.seq_number, sock->curr_seq);
  pack_unsigned_int(pkt.ack_number, sock->curr_ack);
  
  if (network_send_pkt(sock->dst_addr, sizeof(pkt), 
      (char*)&pkt, data_len, data)  == -1) {
    *error = SOCKET_SENDERROR;
  }
}

/* minisocket_resend takes in a resend_arg with fields on the socket on which the send,
 * and what to send.
 * This function will be passed in to an alarm to be called later.
 * If the alarm is called, we will increment try_count, resend and set another alarm 
 * unless we have reached 7 tries, in which case we fail.
 * Note: Interrupts are disabled whenever this function is called,
 * since it is passed in as an alarm func.
 */
void minisocket_resend(void* arg) {
  int wait_cycles;
  resend_arg_t params = (resend_arg_t)arg;
  printf("resend called on try %d\n", params->sock->try_count);
  params->sock->try_count++;
  if (params->sock->try_count >= 7) {
    params->sock->resend_alarm = NULL;
    params->sock->curr_state = EXIT;
    params->sock->try_count = 0;
    semaphore_V(params->sock->pkt_ready_sem);
    return; 
  }
  
  wait_cycles = (1 << params->sock->try_count) * RESEND_TIME_UNIT;
  switch (params->msg_type) {
  case MSG_SYN:
    minisocket_send_ctrl(MSG_SYN, params->sock, params->error);
    break;
  case MSG_SYNACK:
    minisocket_send_ctrl(MSG_SYNACK, params->sock, params->error);
    break;
  case MSG_FIN:
    minisocket_send_ctrl(MSG_FIN, params->sock, params->error);
    break;
  case MSG_ACK:
    minisocket_send_data(params->sock, params->data_len, params->data, params->error);
    break;
  default:
    // error
    return;
  }
  params->sock->resend_alarm = set_alarm(wait_cycles, 
      minisocket_resend, arg, minithread_time());

}


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
  curr_client_idx = CLIENT_START;
}

void minisocket_destroy(minisocket_t sock, minisocket_error* error){
  network_interrupt_arg_t* pkt;

  semaphore_P(server_lock);
  sock_array[sock->src_port] = NULL;
  semaphore_V(server_lock);
  *error = SOCKET_SENDERROR;
  //free all queued packets. interrupts not disabled
  //because socket in array set to null, network
  //handler cannot access queue
  while (queue_dequeue(sock->pkt_q,(void**)&pkt) != -1){
    free(pkt);
  }
  queue_free(sock->pkt_q);
  semaphore_destroy(sock->pkt_ready_sem);
  semaphore_destroy(sock->ack_ready_sem);
  semaphore_destroy(sock->sock_lock);
  free(sock);
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
  /*mini_header_reliable_t pkt_hdr;
  char protocol;
  network_address_t dst_addr;
  unsigned int seq_num;
  unsigned int ack_num;*/
  struct resend_arg resend_alarm_arg;
  

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
    semaphore_destroy(new_sock->pkt_ready_sem);
    free(new_sock);
    *error = SOCKET_OUTOFMEMORY;
    return NULL;
  }
  new_sock->sock_lock = semaphore_create();
  if (!(new_sock->sock_lock)){
    semaphore_V(server_lock);
    free(new_sock->pkt_ready_sem);
    queue_free(new_sock->pkt_q);
    free(new_sock);
    *error = SOCKET_OUTOFMEMORY;
    return NULL;
  }
  new_sock->ack_ready_sem = semaphore_create();
  if (!(new_sock->ack_ready_sem)){
    semaphore_V(server_lock);
    semaphore_destroy(new_sock->pkt_ready_sem);
    queue_free(new_sock->pkt_q);
    semaphore_destroy(new_sock->sock_lock);
    free(new_sock);
    *error = SOCKET_OUTOFMEMORY;
    return NULL;
  }
  semaphore_initialize(new_sock->pkt_ready_sem, 0);
  semaphore_initialize(new_sock->ack_ready_sem, 0);
  semaphore_initialize(new_sock->sock_lock, 1);

  new_sock->curr_state = LISTEN;
  new_sock->try_count = 0;
  new_sock->curr_ack = 0;
  new_sock->curr_seq = 1;
  new_sock->resend_alarm = NULL; // no alarm set
  new_sock->src_port = port;
  new_sock->dst_port = -1; // not paired with client
  network_address_blankify(new_sock->dst_addr); // not paired with client

  sock_array[port] = new_sock;
  semaphore_V(server_lock);
 
  while (new_sock->curr_state != CONNECTED) {
    // wait for MSG_SYN
    semaphore_P(new_sock->ack_ready_sem);
    l = set_interrupt_level(DISABLED);

    /* Sent MSG_SYNACK 7 times w/o response
     * revert to listen state. 
     */
    if (new_sock->curr_state == EXIT) {
      *error = SOCKET_SENDERROR;
      // clean out the queue
      while (queue_dequeue(new_sock->pkt_q,(void**)&pkt) != -1){
        free(pkt);
      }
      new_sock->curr_state = LISTEN;
      new_sock->curr_ack = 0;
      new_sock->curr_seq = 1;
      set_interrupt_level(l);
      continue;
    }

    /* 
    //check packet size at least sizeof tcp header
    if (pkt->size < sizeof(struct mini_header_reliable)) {
      *error = SOCKET_RECEIVEERROR;
      free(pkt);
      continue;
    }
    pkt_hdr = (mini_header_reliable_t)(&pkt->buffer);
    protocol = pkt_hdr->protocol;
    if (protocol != PROTOCOL_MINISTREAM  ){
      *error = SOCKET_RECEIVEERROR;
      free(pkt);
      continue;
    }
    unpack_address(pkt_hdr->source_address, new_sock->dst_addr);
    new_sock->dst_port = unpack_unsigned_short(pkt_hdr->source_port);
    unpack_address(pkt_hdr->destination_address, dst_addr);
    seq_num = unpack_unsigned_int(pkt_hdr->seq_number);
    ack_num = unpack_unsigned_int(pkt_hdr->ack_number);
    if (new_sock->dst_port < CLIENT_START || new_sock->dst_port >= NUM_SOCKETS
          || !network_compare_network_addresses(dst_addr, my_addr) ){
      *error = SOCKET_RECEIVEERROR;
      free(pkt);
      continue;
    }
    */
    switch (new_sock->curr_state) {
    case CONNECTING:  
      minisocket_send_ctrl(MSG_SYNACK, new_sock, error);

      resend_alarm_arg.sock = new_sock;
      resend_alarm_arg.msg_type = MSG_SYNACK;
      resend_alarm_arg.data_len = 0;
      resend_alarm_arg.data = pkt->buffer; //placeholder
      resend_alarm_arg.error = error; 
      new_sock->resend_alarm = set_alarm(RESEND_TIME_UNIT, minisocket_resend, &resend_alarm_arg, minithread_time());
      
      new_sock->curr_state = WAIT;
      set_interrupt_level(l);
      break;

    case WAIT:
      // must have gotten a MSG_ACK 
      new_sock->curr_state = CONNECTED;
      new_sock->try_count = 0;
      new_sock->resend_alarm = NULL;
      deregister_alarm(new_sock->resend_alarm);
      *error = SOCKET_NOERROR;
      free(pkt);
      set_interrupt_level(l);
      break;

    default:
      *error = SOCKET_RECEIVEERROR;
      new_sock->curr_state = EXIT;
      set_interrupt_level(l);
      break;
    }
  }
  return new_sock;
}

/* Initiate the communication with a remote site. When communication is
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
  minisocket_t new_sock;
  interrupt_level_t l;
  struct resend_arg resend_alarm_arg;
  unsigned short start; 
  char tmp;

  //check valid portnum
  if (port < 0 || port >= CLIENT_START) {
    *error = SOCKET_INVALIDPARAMS;
    return NULL;
  }

  semaphore_P(client_lock);
  start = curr_client_idx;
  while (sock_array[curr_client_idx] != NULL){
    curr_client_idx++;
    if (curr_client_idx >= NUM_SOCKETS){
      curr_client_idx = CLIENT_START;
    }
    if (curr_client_idx == start){ // client sockets full
      semaphore_V(client_lock);
      *error = SOCKET_NOMOREPORTS; 
      return NULL;
    }
  }
 
  new_sock = (minisocket_t)malloc(sizeof(struct minisocket));
  if (!new_sock){
    semaphore_V(client_lock);
    *error = SOCKET_OUTOFMEMORY;
    return NULL;
  }
  new_sock->pkt_ready_sem = semaphore_create();
  if (!(new_sock->pkt_ready_sem)){
    semaphore_V(client_lock);
    free(new_sock);
    *error = SOCKET_OUTOFMEMORY;
    return NULL;
  }
  new_sock->pkt_q = queue_new();
  if (!(new_sock->pkt_q)){
    semaphore_V(client_lock);
    semaphore_destroy(new_sock->pkt_ready_sem);
    free(new_sock);
    *error = SOCKET_OUTOFMEMORY;
    return NULL;
  }
  new_sock->sock_lock = semaphore_create();
  if (!(new_sock->sock_lock)){
    semaphore_V(client_lock);
    semaphore_destroy(new_sock->pkt_ready_sem);
    queue_free(new_sock->pkt_q);
    free(new_sock);
    *error = SOCKET_OUTOFMEMORY;
    return NULL;
  }
  new_sock->ack_ready_sem = semaphore_create();
  if (!(new_sock->ack_ready_sem)){
    semaphore_V(client_lock);
    semaphore_destroy(new_sock->pkt_ready_sem);
    queue_free(new_sock->pkt_q);
    semaphore_destroy(new_sock->sock_lock);
    free(new_sock);
    *error = SOCKET_OUTOFMEMORY;
    return NULL;
  }
  
  semaphore_initialize(new_sock->pkt_ready_sem, 0);
  semaphore_initialize(new_sock->ack_ready_sem, 0);
  semaphore_initialize(new_sock->sock_lock, 1);

  new_sock->curr_state = CONNECT_WAIT;
  new_sock->try_count = 0;
  new_sock->curr_ack = 0;
  new_sock->curr_seq = 1;
  new_sock->resend_alarm = NULL; //no alarm set
  new_sock->src_port = curr_client_idx;
  new_sock->dst_port = port; 
  network_address_copy(addr, new_sock->dst_addr);

  sock_array[curr_client_idx++] = new_sock;
  semaphore_V(client_lock);
 
  minisocket_send_ctrl(MSG_SYN, new_sock, error);
  resend_alarm_arg.sock = new_sock;
  resend_alarm_arg.msg_type = MSG_SYN;
  resend_alarm_arg.data_len = 0;
  resend_alarm_arg.data = &tmp; //placeholder
  resend_alarm_arg.error = error; 
  new_sock->resend_alarm = set_alarm(RESEND_TIME_UNIT, minisocket_resend, &resend_alarm_arg, minithread_time());

  while (new_sock->curr_state != CONNECTED) {
    semaphore_P(new_sock->ack_ready_sem);
    l = set_interrupt_level(DISABLED);

    /* Sent MSG_SYN 7 times w/o response
     * FAIL and return. 
     */
    if (new_sock->curr_state == EXIT){
      *error = SOCKET_NOSERVER;
      minisocket_destroy(new_sock, error);
      set_interrupt_level(l); 
      return NULL;
    }

    switch(new_sock->curr_state) {
    case CONNECT_WAIT: 
      // must have gotten a MSG_SYNACK
      new_sock->curr_state = CONNECTED;
      new_sock->try_count = 0;
      new_sock->resend_alarm = NULL;
      deregister_alarm(new_sock->resend_alarm);
      *error = SOCKET_NOERROR;
      minisocket_send_ctrl(MSG_ACK, new_sock, error);
      set_interrupt_level(l); 
      break;
    
    default:
      // error
      *error = SOCKET_RECEIVEERROR;
      new_sock->curr_state = EXIT;
      set_interrupt_level(l); 
      break;
    }
  }
  return new_sock;
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

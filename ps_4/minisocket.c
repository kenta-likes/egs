/*
 *	Implementation of minisockets.
 */
#include "minisocket.h"
#include <stdlib.h>
#include <stdio.h>


typedef enum state {LISTEN = 1, CONNECTING, CONNECT_WAIT, MSG_WAIT, 
    CLOSE_SEND, CLOSE_RCV, CONNECTED, EXIT} state;


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
} resend_arg;

typedef resend_arg* resend_arg_t;

minisocket_t* sock_array;
semaphore_t client_lock;
semaphore_t server_lock;
network_address_t my_addr;
unsigned int curr_client_idx;


//self destruct function
void self_destruct(void* arg);

/* minisocket_resend takes in a resend_arg with fields on the socket on which the send,
 * and what to send.
 * This function will be passed in to an alarm to be called later.
 * If the alarm is called, we will increment try_count, resend and set another alarm 
 * unless we have reached 7 tries, in which case we fail.
 * Note: Interrupts are disabled whenever this function is called,
 * since it is passed in as an alarm func.
 */
void minisocket_resend(void* arg);

void minisocket_send_ctrl_to(char type, minisocket_t sock, minisocket_error* error, network_address_t to_addr, unsigned short to_port) {
  struct mini_header_reliable pkt;
  pkt.protocol = PROTOCOL_MINISTREAM;
  pack_address(pkt.source_address, my_addr);
  pack_unsigned_short(pkt.source_port, sock->src_port);
  pack_address(pkt.destination_address, to_addr);
  pack_unsigned_short(pkt.destination_port, to_port);
  pkt.message_type = type;
  pack_unsigned_int(pkt.seq_number, sock->curr_seq);
  pack_unsigned_int(pkt.ack_number, sock->curr_ack);
  
  if (network_send_pkt(to_addr, sizeof(pkt), 
      (char*)&pkt, 0, (char*)&pkt) == -1) {
    *error = SOCKET_SENDERROR;
  }  
}

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
  printf("resend called on %li\n", (long)arg);
  //printf("resend called during state %i on try %d\n", params->sock->curr_state, params->sock->try_count);
  params->sock->try_count++;
  if (params->sock->try_count >= 7) {
    params->sock->resend_alarm = NULL;
    params->sock->curr_state = EXIT;
    params->sock->try_count = 0;
    semaphore_V(params->sock->ack_ready_sem);//notify to unblock
    //semaphore_V(params->sock->ack_ready_sem);//notify to unblock sender
    //semaphore_V(params->sock->ack_ready_sem);//notify to unblock close
    return; 
  }
  //printf("in resend_alarm, still have tries left.\n"); 
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
  //printf("Failing right after here?\n");
  params->sock->resend_alarm = set_alarm(wait_cycles, 
      minisocket_resend, arg, minithread_time());
  //printf("exit resend. No seg fault here. Try %d\n", params->sock->try_count);
}


/* Initializes the minisocket layer. */
void minisocket_initialize()
{
  int i;
  sock_array = (minisocket_t*)malloc(sizeof(struct minisocket)*NUM_SOCKETS);
  if (!sock_array) {
    return;
  }
  for (i = 0; i < NUM_SOCKETS; i++) {
    sock_array[i] = NULL;
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
  //printf("minisocket_initialize complete\n");
}

void minisocket_destroy(minisocket_t sock, minisocket_error* error){
  network_interrupt_arg_t* pkt;

  semaphore_P(server_lock);
  sock_array[sock->src_port] = NULL;
  semaphore_V(server_lock);
  *error = SOCKET_NOERROR;
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

void self_destruct(void* arg) {
  minisocket_error error;
  minisocket_destroy((minisocket_t)arg, &error);
  //printf("Goodby cruel world\n");
  return;
}

/* Listen for a connection from somebody else. When communication link is
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
  resend_arg* resend_alarm_arg;
  char tmp; 

  //check valid portnum
  if (port < 0 || port >= CLIENT_START){
    *error = SOCKET_INVALIDPARAMS;
    return NULL;
  }

  //check port in use
  semaphore_P(server_lock);

  //printf("calling server_create at port %d.\n", port);
  //printf("value at port %d is %li.\n", port, (long)sock_array[port]);
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
 
  resend_alarm_arg = (resend_arg*)calloc(1, sizeof(resend_arg));
  while (1) {
    // wait for MSG_SYN
    semaphore_P(new_sock->ack_ready_sem);
    l = set_interrupt_level(DISABLED);
 
    switch (new_sock->curr_state) {
    case CONNECTING:  
      minisocket_send_ctrl(MSG_SYNACK, new_sock, error);

      resend_alarm_arg->sock = new_sock;
      resend_alarm_arg->msg_type = MSG_SYNACK;
      resend_alarm_arg->data_len = 0;
      resend_alarm_arg->data = &tmp; //placeholder
      resend_alarm_arg->error = error; 
      new_sock->resend_alarm = set_alarm(RESEND_TIME_UNIT, minisocket_resend, resend_alarm_arg, minithread_time());
      
      new_sock->curr_state = MSG_WAIT;
      set_interrupt_level(l);
      break;

    case CONNECTED:
      // must have gotten a MSG_ACK 
      //printf("in server_create, SUCCESS!\n");
      new_sock->try_count = 0;
      deregister_alarm(new_sock->resend_alarm);
      new_sock->resend_alarm = NULL;
      *error = SOCKET_NOERROR;
      set_interrupt_level(l);
      //printf("exiting server_create\n");
      free(resend_alarm_arg);
      return new_sock;
      break;

    case EXIT: default:
      *error = SOCKET_SENDERROR;
      // clean out the queue
      while (queue_dequeue(new_sock->pkt_q,(void**)&pkt) != -1){
        free(pkt);
      }
      new_sock->curr_state = LISTEN;
      new_sock->curr_ack = 0;
      new_sock->curr_seq = 1;
      set_interrupt_level(l);
      break;
    }
  }
  free(resend_alarm_arg);
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
  resend_arg* resend_alarm_arg;
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

  sock_array[curr_client_idx] = new_sock;
  semaphore_V(client_lock);
 
  l = set_interrupt_level(DISABLED);
  minisocket_send_ctrl(MSG_SYN, new_sock, error);
  resend_alarm_arg = (resend_arg*)calloc(1, sizeof(resend_arg));
  resend_alarm_arg->sock = new_sock;
  resend_alarm_arg->msg_type = MSG_SYN;
  resend_alarm_arg->data_len = 0;
  resend_alarm_arg->data = &tmp; //placeholder
  resend_alarm_arg->error = error; 
  new_sock->resend_alarm = set_alarm(RESEND_TIME_UNIT, minisocket_resend, resend_alarm_arg, minithread_time());
  set_interrupt_level(l);

  semaphore_P(new_sock->ack_ready_sem);
  l = set_interrupt_level(DISABLED);

  switch(new_sock->curr_state) {
  case CONNECTED: //we are connected
    // must have gotten a MSG_SYNACK
    new_sock->curr_state = CONNECTED;
    new_sock->try_count = 0;
    if (new_sock->resend_alarm){
      deregister_alarm(new_sock->resend_alarm);
    }
    new_sock->resend_alarm = NULL;
    *error = SOCKET_NOERROR;
    set_interrupt_level(l); 
    break;
  
  case CLOSE_RCV:
    *error = SOCKET_BUSY;
    minisocket_destroy(new_sock, error);
    if (new_sock->resend_alarm){
      deregister_alarm(new_sock->resend_alarm);
    }
    new_sock->resend_alarm = NULL;
    *error = SOCKET_BUSY;
    set_interrupt_level(l);
    new_sock = NULL;
    break; 
  
  default:
    // error
    *error = SOCKET_NOSERVER;
    minisocket_destroy(new_sock, error);
    set_interrupt_level(l); 
    new_sock = NULL;
    break;
  }
  free(resend_alarm_arg);
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
 *
 * Comments: invalid params for socket, msg but len = 0 case is ok
 */
int minisocket_send(minisocket_t socket, minimsg_t msg, int len, minisocket_error *error)
{
  unsigned int num_pkt;
  unsigned int i;
  interrupt_level_t l;
  resend_arg* resend_alarm_arg;
  unsigned int max_size;

  //printf("in minisocket_send\n");
  //max size for PAYLOAD, not entire packet
  max_size = MAX_NETWORK_PKT_SIZE - sizeof(struct mini_header_reliable);
  num_pkt = len / max_size + 1; //number of divided packets
  //printf("gonna send %d packet(s)!\n", num_pkt);
  //error checking
  if (!socket || sock_array[socket->src_port] == NULL || msg == NULL 
      || len < 0)
  {
    *error = SOCKET_INVALIDPARAMS;
    return -1;
  }
  //allow this
  if (len == 0){
    return 0;
  }

  //check for invalid cases
  l = set_interrupt_level(DISABLED);
  if (socket->curr_state != CONNECTED){
    if (socket->curr_state == CLOSE_RCV){
      *error = SOCKET_BUSY;
      set_interrupt_level(l);
      return -1;
    }
    *error = SOCKET_SENDERROR;
    set_interrupt_level(l);
    return -1;
  }

  //try sending
  *error = SOCKET_NOERROR;
  (socket->curr_seq)++;
  minisocket_send_data(socket,len>max_size ? max_size:len,msg, error);
  //printf("sent first packet!\n");
  if (*error != SOCKET_NOERROR){
    set_interrupt_level(l);
    return -1;
  }

  resend_alarm_arg = (resend_arg*)calloc(1, sizeof(resend_arg));

  //otherwise register an alarm for resending and proceed
  resend_alarm_arg->sock = socket;
  resend_alarm_arg->msg_type = MSG_ACK;
  resend_alarm_arg->data_len = len>max_size?max_size:len;
  resend_alarm_arg->data = msg; //placeholder
  resend_alarm_arg->error = error; 
  socket->resend_alarm = set_alarm(RESEND_TIME_UNIT, minisocket_resend, resend_alarm_arg, minithread_time());

  if (socket->resend_alarm){
    deregister_alarm(socket->resend_alarm);
    socket->resend_alarm = NULL;
  }
  socket->resend_alarm = set_alarm(RESEND_TIME_UNIT, minisocket_resend, &resend_alarm_arg, minithread_time());
  socket->curr_state = MSG_WAIT;
  set_interrupt_level(l);


  while (i < num_pkt){
    semaphore_P(socket->ack_ready_sem);
    l = set_interrupt_level(DISABLED);
    //packet got acked, deregister alarm from before
    if (socket->resend_alarm){
      deregister_alarm(socket->resend_alarm);
      socket->resend_alarm = NULL;
    }

    switch (socket->curr_state){
    case CLOSE_SEND: //closing connection
      semaphore_V(socket->ack_ready_sem); //notify close thread
      *error = SOCKET_SENDERROR;
      set_interrupt_level(l);
      free(resend_alarm_arg); 
      return i * max_size;
    case CONNECTED: 
      i++;
      if (i >= num_pkt){ //we sent everything
        set_interrupt_level(l);
        break;
      }
      (socket->curr_seq)++;
      minisocket_send_data(socket,
              i<num_pkt-1? max_size : len-(max_size*i),
              ((char*)msg) + i*max_size,
              error);

      if (*error != SOCKET_NOERROR){//TODO:may not need to do this
        set_interrupt_level(l);
        free(resend_alarm_arg); 
        return -1;
      }
      //otherwise register an alarm for resending and proceed
      resend_alarm_arg->sock = socket;
      resend_alarm_arg->msg_type = MSG_ACK;
      resend_alarm_arg->data_len = i<num_pkt-1? max_size : len-(max_size*i);
      resend_alarm_arg->data = ((char*)msg) + i*max_size;
      resend_alarm_arg->error = error;
      if (socket->resend_alarm) {
        deregister_alarm(socket->resend_alarm);
        socket->resend_alarm = NULL;
      }
      socket->resend_alarm = set_alarm(RESEND_TIME_UNIT, minisocket_resend, resend_alarm_arg, minithread_time());
      socket->curr_state = MSG_WAIT;
      set_interrupt_level(l);
      break;
    default: // error case, simply return
      *error = SOCKET_SENDERROR;
      set_interrupt_level(l);
      free(resend_alarm_arg); 
      return i * max_size;
    }
  }
  //we sent all the packets!
  *error = SOCKET_NOERROR;
  //printf("return on send SUCCESS!\n");
  //printf("exiting send with socket at state %d\n", socket->curr_state);
  free(resend_alarm_arg); 
  return len;
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
  network_interrupt_arg_t* pkt;
  char* data;
  int data_len;
  interrupt_level_t l;
  
  //printf("in receive!\n");
  semaphore_P(socket->sock_lock);
  semaphore_P(socket->pkt_ready_sem);

  l = set_interrupt_level(DISABLED);
  if (queue_dequeue(socket->pkt_q, (void**)&pkt) == -1) {
    *error = SOCKET_SENDERROR;
    semaphore_V(socket->sock_lock);
    set_interrupt_level(l);
    return -1;
  }

  //printf("got my packet yo!\n");
  if (socket->curr_state == CONNECTED || socket->curr_state == MSG_WAIT) {
    data = (char*)(pkt->buffer) + sizeof(struct mini_header_reliable);
    data_len = pkt->size - sizeof(struct mini_header_reliable);
    //printf("my data has len %d\n", data_len);
    if (data_len > max_len) {
      data_len = max_len;
    }
    memcpy(msg, data, data_len);
    free(pkt);
    *error = SOCKET_NOERROR; 
    semaphore_V(socket->sock_lock);
    set_interrupt_level(l);
    //printf("got my data...VICTORY!\n");
    return data_len;
  }
  else {
    free(pkt);
    *error = SOCKET_RECEIVEERROR;
    semaphore_V(socket->sock_lock);
    set_interrupt_level(l);
    return -1;
  }
}

/* Close a connection. If minisocket_close is issued, any send or receive should
 * fail.  As soon as the other side knows about the close, it should fail any
 * send or receive in progress. The minisocket is destroyed by minisocket_close
 * function.  The function should never fail.
 */
void minisocket_close(minisocket_t socket)
{
  interrupt_level_t l;
  minisocket_error error;
  struct resend_arg resend_alarm_arg;
  error = SOCKET_NOERROR;

  //printf("in minisocket_close\n");
  
  //set state to close_wait
  //send close, wait using alarms
  //(after 7 retries, close connection)
  //P on ack_ready
  //if ack_ready, close connection
 
  //ill formed argument 
  if (!sock_array){
    return;
  }

  l = set_interrupt_level(DISABLED);
  //ill formed argument
  if (sock_array[socket->src_port] == NULL){
    set_interrupt_level(l);
    return;
  }
  if (socket->curr_state == CLOSE_SEND || socket->curr_state == CLOSE_RCV){
    set_interrupt_level(l);
    return;
  }
  socket->curr_state = CLOSE_SEND;
  socket->try_count = 0;
  socket->curr_seq++;

  minisocket_send_ctrl( MSG_FIN, socket, &error);
  //printf("in minisocket_close, sent my MSG_FIN\n");
  resend_alarm_arg.sock = socket;
  resend_alarm_arg.msg_type = MSG_FIN;
  resend_alarm_arg.data_len = 0;
  resend_alarm_arg.data = (char*) &socket; //placeholder
  resend_alarm_arg.error = &error;
  if (socket->resend_alarm){
    deregister_alarm(socket->resend_alarm);
  }
  socket->resend_alarm = NULL;
  socket->resend_alarm = set_alarm(RESEND_TIME_UNIT, minisocket_resend, &resend_alarm_arg, minithread_time());
  //printf("in minisocket_close, set my alarm.\n");
  set_interrupt_level(l);

  semaphore_P(socket->ack_ready_sem); //wait for ack packet...
  //received ack, close connection
  //printf("in minisocket_close, got my ACK baby\n");
  l = set_interrupt_level(DISABLED);
  if (socket->resend_alarm){
    deregister_alarm(socket->resend_alarm);
  }
  socket->resend_alarm = NULL;

  minisocket_destroy(socket, &error);
  //printf("Closed. Error is %d", error);
  if (error != SOCKET_NOERROR){
    //printf("Something went wrong. Close connection failure.\n");
  }
  set_interrupt_level(l);

  //printf("in minisocket_close, SUCCESS\n");
  return;

}


/* minisocket_process_packet is called within the network interrupt handler.
 * Therefore we can safely assume that interrupts are disabled.
 *
 */
void minisocket_process_packet(void* packet) {
  network_interrupt_arg_t* pkt;
  mini_header_reliable_t pkt_hdr;
  unsigned int seq_num;
  unsigned int ack_num;
  minisocket_error error;
  network_address_t src_addr;
  network_address_t dst_addr;
  unsigned int src_port;
  unsigned int dst_port;
  minisocket_t sock;
  int type;
  int data_len;

  pkt = (network_interrupt_arg_t*)packet;
  pkt_hdr = (mini_header_reliable_t)(&pkt->buffer);
 
  //printf("in minisocket_process_packet\n");
  // error checking
  if (pkt->size < sizeof(struct mini_header_reliable)) {
    free(pkt);
    return;
  }

  unpack_address(pkt_hdr->source_address, src_addr);
  src_port = unpack_unsigned_short(pkt_hdr->source_port);
  dst_port = unpack_unsigned_short(pkt_hdr->destination_port);
  unpack_address(pkt_hdr->destination_address, dst_addr);
  seq_num = unpack_unsigned_int(pkt_hdr->seq_number);
  ack_num = unpack_unsigned_int(pkt_hdr->ack_number);
  data_len = pkt->size - sizeof(struct mini_header_reliable);
  if (src_port < 0 || dst_port < 0 
        || src_port >= NUM_SOCKETS || dst_port >= NUM_SOCKETS
        || !network_compare_network_addresses(dst_addr, my_addr)){
    free(pkt);
    return;
  }

  error = SOCKET_NOERROR;
  sock = sock_array[dst_port];
  if (sock == NULL) {
    free(pkt);
    return;
  }

  type = pkt_hdr->message_type;
  //printf("socket %d with state %d\n", sock->src_port, sock->curr_state);
  //printf("pkt ack number is %d\n", ack_num);
  //printf("pkt seq number is %d\n", seq_num);
  //printf("my ack number is %d\n", sock->curr_ack);
  //printf("my seq number is %d\n", sock->curr_seq);
  //printf("type of msg received is %d\n", type);

  if (sock->curr_state != LISTEN &&
        ( !network_compare_network_addresses(src_addr, sock->dst_addr)
            || src_port != sock->dst_port ) ){
    if (type == MSG_SYN){
      minisocket_send_ctrl_to(MSG_FIN, sock, &error, src_addr, src_port);
    }
    free(pkt);
    return;
  }

  switch (sock->curr_state) {
    case LISTEN:
      if (type == MSG_SYN) {
        sock->curr_ack = 1;
        semaphore_V(sock->ack_ready_sem);
        sock->curr_state = CONNECTING;
        sock->dst_port = src_port;
        network_address_copy(src_addr, sock->dst_addr);
      }
      free(pkt);
      break;
    
    case CONNECTING:
      if (type == MSG_ACK) {
        if (ack_num == sock->curr_seq) {
        semaphore_V(sock->ack_ready_sem);
        sock->curr_state = CONNECTED;
        }
        if (seq_num == sock->curr_ack+1 && data_len > 0) {
          queue_append(sock->pkt_q, pkt);
          semaphore_V(sock->pkt_ready_sem);
          minisocket_send_ctrl(MSG_ACK, sock, &error);
          sock->curr_ack++;
        }
        else {
          free(pkt);
        }
      }
      else {
        free(pkt);
      }
      break;
    
    case CONNECT_WAIT://TODO
      if (type == MSG_FIN) {
        sock->curr_state = CLOSE_RCV;
        semaphore_V(sock->ack_ready_sem);//notify blocked guy
      }
      else if (type == MSG_SYNACK) {
        if (ack_num == sock->curr_seq) {
          sock->curr_state = CONNECTED;
          sock->curr_ack++;
          minisocket_send_ctrl(MSG_ACK, sock, &error);
          semaphore_V(sock->ack_ready_sem);
        }
      }       
      free(pkt); 
      break;
    
    case MSG_WAIT:
      if (type == MSG_FIN){
        sock->curr_ack++;
        minisocket_send_ctrl(MSG_ACK, sock, &error);//notify to closer
        sock->curr_state = CLOSE_RCV;

        if (sock->resend_alarm){
          deregister_alarm(sock->resend_alarm);
          sock->resend_alarm = NULL;
        }
        sock->resend_alarm = set_alarm(RESEND_TIME_UNIT * 150, 
                                          self_destruct, 
                                          (void*)sock, 
                                          minithread_time());
        semaphore_V(sock->ack_ready_sem);//notify blocked guy
      }
      else if (type == MSG_ACK) {
        if (ack_num == sock->curr_seq) {
          //printf("got an ACK!\n");
          semaphore_V(sock->ack_ready_sem);
          sock->curr_state = CONNECTED;
        }
        if (seq_num == sock->curr_ack+1 && data_len > 0) {
          //printf("got a MESSAGE!\n");
          queue_append(sock->pkt_q, pkt);
          sock->curr_ack++;
          minisocket_send_ctrl(MSG_ACK, sock, &error);
          semaphore_V(sock->pkt_ready_sem);
        }
        else {
          free(pkt);
        }
      }
      else {
        free(pkt);
      }        
      break;

    case CLOSE_SEND:
      if (type == MSG_ACK && ack_num == sock->curr_seq) {
          semaphore_V(sock->ack_ready_sem);
      }
      if (type == MSG_FIN){
        sock->curr_ack++;
        minisocket_send_ctrl(MSG_ACK, sock, &error);//notify to closer
        sock->curr_state = CLOSE_RCV;

        if (sock->resend_alarm){
          deregister_alarm(sock->resend_alarm);
          sock->resend_alarm = NULL;
        }
        sock->resend_alarm = set_alarm(RESEND_TIME_UNIT * 150, 
                                          self_destruct, 
                                          (void*)sock, 
                                          minithread_time());
        //minisocket_send_ctrl(MSG_ACK, sock, &error);
      }
      //free(pkt);
      break;

    case CLOSE_RCV:
      if (type == MSG_FIN && ack_num == sock->curr_seq) {
        minisocket_send_ctrl(MSG_ACK, sock, &error);
        //semaphore_V(sock->ack_ready_sem);
      }
      free(pkt);
      break;

    case CONNECTED:
      if (type == MSG_FIN){
        sock->curr_ack++;
        minisocket_send_ctrl(MSG_ACK, sock, &error);//notify to closer
        sock->curr_state = CLOSE_RCV;

        if (sock->resend_alarm){
          deregister_alarm(sock->resend_alarm);
          sock->resend_alarm = NULL;
        }
        sock->resend_alarm = set_alarm(RESEND_TIME_UNIT * 150, 
                                          self_destruct, 
                                          (void*)sock, 
                                          minithread_time());
      }
      if (type == MSG_ACK) {
        if (ack_num == sock->curr_seq) {
          //semaphore_V(sock->ack_ready_sem);
          sock->curr_state = CONNECTED;
        }
        if (seq_num == sock->curr_ack+1 && data_len > 0) {
          //printf("got some DATA\n"); 
          queue_append(sock->pkt_q, pkt);
          sock->curr_ack++;
          minisocket_send_ctrl(MSG_ACK, sock, &error);
          semaphore_V(sock->pkt_ready_sem);
          //printf("got data, no seg fault\n");
        }
        else if (seq_num == sock->curr_ack && data_len > 0){
          minisocket_send_ctrl(MSG_ACK, sock, &error);
        }
        else {
          free(pkt);
        }
      }
      else {
        free(pkt);
      }
      break;

    case EXIT: default:
      free(pkt);
      break;
    }
}


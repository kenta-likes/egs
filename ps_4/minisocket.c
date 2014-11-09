/*
 *	Implementation of minisockets.
 */
#include "minisocket.h"
#include <stdlib.h>
#include <stdio.h>
#include "synch.h"
#include "queue.h"
#include "alarm.h"
#include "network.h"

#define NUM_SOCKETS 65536
#define SERVER_START 0
#define CLIENT_START 32768

typedef enum state { LISTEN = 1, CONNECTED, WAIT, EXIT} state;

struct minisocket
{
  state curr_state;
  int try_count;
  int curr_ack;
  int curr_seq;
  alarm_t resend_alarm; 
  semaphore_t ack_rcv;
  semaphore_t pkt_ready; 
  queue_t pkt_q;
  int src_port;
  network_address_t dst_addr;
  int dst_port;
};

minisocket_t* sock_array;
semaphore_t client_lock;
semaphore_t server_lock;
network_address_t src_addr;

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
  network_get_my_address(src_addr);
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
  return NULL;
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

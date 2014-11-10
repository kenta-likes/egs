/* 
 *    conn-network test program 4
 *
 *    spawns two threads, both of which send a big message
 *    and receive the big messages
*/

#include "defs.h"
#include "minithread.h"
#include "minisocket.h"
#include "synch.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#define BUFFER_SIZE 100000

int port=80; /* port on which we do the communication */

int receive(int* arg); /* forward declaration */

int send_packets(int* arg); //forward declaration
int receive_packets(int* arg); //forward declaration

semaphore_t server_receive_sem;
semaphore_t server_send_sem;
semaphore_t client_receive_sem;
semaphore_t client_send_sem;

char* GetErrorDescription(int errorcode){
  switch(errorcode){
  case SOCKET_NOERROR:
    return "No error reported";
    break;

  case SOCKET_NOMOREPORTS:
    return "There are no more ports available";
    break;

  case SOCKET_PORTINUSE:
    return "The port is already in use by the server";
    break;

  case SOCKET_NOSERVER:
    return "No server is listening";
    break;

  case SOCKET_BUSY:
    return "Some other client already connected to the server";
    break;
    
  case SOCKET_SENDERROR:
    return "Sender error";
    break;

  case SOCKET_RECEIVEERROR:
    return "Receiver error";
    break;
    
  default:
    return "Unknown error";
  }
}

/*
 * receive function for SERVER's receive thread
 * */
int receive_packets(int* arg){
  char buffer[BUFFER_SIZE];
  int i;
  int bytes_received;
  minisocket_t socket;
  minisocket_error error;

  socket = (minisocket_t)arg;

  /* receive the message */
  bytes_received=0;
  while (bytes_received!=BUFFER_SIZE){
    int received_bytes;
    if ((received_bytes=minisocket_receive(socket,buffer, BUFFER_SIZE-bytes_received, &error))==-1){
      printf("ERROR: %s. Exiting. \n",GetErrorDescription(error));
      /* close the connection */
      minisocket_close(socket);
      semaphore_V(server_send_sem); //sender thread
      return -1;
    }   
    /* test the information received */
    for (i=0; i<received_bytes; i++){
      if (buffer[i]!=(char)( (bytes_received+i)%256 )){
	printf("The %d'th byte received is wrong.\n",
	       bytes_received+i);
	/* close the connection */
	minisocket_close(socket);
  semaphore_V(server_send_sem); //sender thread
	return -1;
      }
    }
	      
    bytes_received+=received_bytes;
  }

  //printf("All bytes received correctly.\n");

  semaphore_V(server_send_sem); //sender thread
  semaphore_P(server_receive_sem); //myself
  return 0;

}

int transmit(int* arg) {
  char buffer[BUFFER_SIZE];
  int i;
  int bytes_sent;
  minisocket_t socket;
  minisocket_error error;

  minithread_fork(receive, NULL);
  // receiver = minithread_fork(receive, NULL);

  socket = minisocket_server_create(port,&error);
  if (socket==NULL){
    printf("ERROR: %s. Exiting. \n",GetErrorDescription(error));
    semaphore_V(server_receive_sem); //receiver thread
    return -1;
  }

  //create server's receive socket
  minithread_fork(receive_packets, (int*)socket);

  /* Fill in the buffer with numbers from 0 to BUFFER_SIZE-1 */
  for (i=0; i<BUFFER_SIZE; i++){
    buffer[i]=(char)(i%256);
  }

  /* send the message */
  bytes_sent=0;
  while (bytes_sent!=BUFFER_SIZE){
    int trans_bytes=
      minisocket_send(socket,buffer+bytes_sent,
		      BUFFER_SIZE-bytes_sent, &error);
  
    printf("Sent %d bytes.\n",trans_bytes);

    if (error!=SOCKET_NOERROR){
      printf("ERROR: %s. Exiting. \n",GetErrorDescription(error));
      /* close the connection */
      minisocket_close(socket);
    
      semaphore_V(server_receive_sem); //receiver thread
      return -1;
    }   

    bytes_sent+=trans_bytes;
  }

  /* close the connection. make sure send and receive end together */
  semaphore_V(server_receive_sem); //receiver thread
  semaphore_P(server_send_sem); //myself
  minisocket_close(socket);

  return 0;
}

/*
 * send method for CLIENT's send thread
 * */
int send_packets(int* arg){
  char buffer[BUFFER_SIZE];
  int i;
  int bytes_sent;
  minisocket_t socket; //socket sending from
  minisocket_error error;

  socket = (minisocket_t)arg;

  /* Fill in the buffer with numbers from 0 to BUFFER_SIZE-1 */
  for (i=0; i<BUFFER_SIZE; i++){
    buffer[i]=(char)(i%256);
  }

  /* send the message */
  bytes_sent=0;
  while (bytes_sent!=BUFFER_SIZE){
    int trans_bytes=
      minisocket_send(socket,buffer+bytes_sent,
		      BUFFER_SIZE-bytes_sent, &error);
  
    printf("Sent %d bytes.\n",trans_bytes);

    if (error!=SOCKET_NOERROR){
      printf("ERROR: %s. Exiting. \n",GetErrorDescription(error));
      /* close the connection */
      minisocket_close(socket);
    
      semaphore_V(client_receive_sem); //receiving thread
      return -1;
    }   

    bytes_sent+=trans_bytes;
  }
  semaphore_V(client_receive_sem); //receiving thread
  semaphore_P(client_send_sem); //myself
  
  return 0;

}

int receive(int* arg) {
  char buffer[BUFFER_SIZE];
  int i;
  int bytes_received;
  network_address_t my_address;
  minisocket_t socket;
  minisocket_error error;

  /* 
   * It is crucial that this minithread_yield() works properly
   * (i.e. it really gives the processor to another thread)
   * othrevise the client starts before the server and it will
   * fail (there is nobody to connect to).
   */
  minithread_yield();
  
  network_get_my_address(my_address);
  
  /* create a network connection to the local machine */
  socket = minisocket_client_create(my_address, port,&error);
  if (socket==NULL){
    printf("ERROR: %s. Exiting. \n",GetErrorDescription(error));
    semaphore_V(client_send_sem); //sending thread
    return -1;
  }

  //create client's send socket
  minithread_fork(send_packets, (int*)socket);

  /* receive the message */
  bytes_received=0;
  while (bytes_received!=BUFFER_SIZE){
    int received_bytes;
    if ((received_bytes=minisocket_receive(socket,buffer, BUFFER_SIZE-bytes_received, &error))==-1){
      printf("ERROR: %s. Exiting. \n",GetErrorDescription(error));
      /* close the connection */
      minisocket_close(socket);
      semaphore_V(client_send_sem); //sending thread
      return -1;
    }   
    /* test the information received */
    for (i=0; i<received_bytes; i++){
      if (buffer[i]!=(char)( (bytes_received+i)%256 )){
	printf("The %d'th byte received is wrong.\n",
	       bytes_received+i);
	/* close the connection */
	minisocket_close(socket);
  semaphore_V(client_send_sem); //sending thread
	return -1;
      }
    }
	      
    bytes_received+=received_bytes;
  }

  printf("All bytes received correctly.\n");

  semaphore_V(client_send_sem); //sending thread
  semaphore_P(client_receive_sem); //myself
  
  minisocket_close(socket);

  return 0;
}

int main(int argc, char** argv) {
  semaphore_create(server_receive_sem);
  if (!server_receive_sem){
    return -1;
  }
  semaphore_create(server_send_sem);
  if (!server_send_sem){
    semaphore_destroy(server_receive_sem);
    return -1;
  }
  semaphore_create(client_receive_sem);
  if (!client_receive_sem){
    semaphore_destroy(server_receive_sem);
    semaphore_destroy(server_send_sem);
    return -1;
  }
  semaphore_create(client_send_sem);
  if (!client_send_sem){
    semaphore_destroy(server_receive_sem);
    semaphore_destroy(server_send_sem);
    semaphore_destroy(client_send_sem);
    return -1;
  }
  semaphore_initialize(server_receive_sem, 0);
  semaphore_initialize(server_send_sem, 0);
  semaphore_initialize(client_receive_sem, 0);
  semaphore_initialize(client_send_sem, 0);

  minithread_system_initialize(transmit, NULL);
  return -1;
}

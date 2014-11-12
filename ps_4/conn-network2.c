/* 
 *    conn-network test program 2
 *
 *    send a big message between two processes on different computers
 *
 *    usage: conn-network2 [<hostname>]
 *    if no hostname is supplied, will wait for a connection and then transmit a big message
 *    if a hostname is given, will make a connection and then receive
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
char* hostname;

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

int server(int* arg) {
  minisocket_error error;
  minisocket_t socket;
  char* msg;
  int data_len;

  msg = "hello";
  data_len = strlen(msg) + 1;
  socket = minisocket_server_create(port,&error);
  printf("made a server\n");
  if (socket==NULL){
    printf("ERROR: %s. Exiting. \n",GetErrorDescription(error));
    return -1;
  }
  minisocket_send(socket, msg, data_len, &error);

  return 0;
}

int client(int* arg) {
  network_address_t address;
  minisocket_t socket;
  minisocket_error error;
  char msg[strlen("hello") + 1]; 
 
  network_translate_hostname(hostname, address);
  
  /* create a network connection to the local machine */
  socket = minisocket_client_create(address, port,&error);
  printf("made a client\n");

  minisocket_receive(socket, msg, strlen("hello")+1, &error);
  return 0;
}


int main(int argc, char** argv) {

  if (argc > 1) {
    hostname = argv[1];
    minithread_system_initialize(client, NULL);
  }
  else {
    minithread_system_initialize(server, NULL);
  }
  return -1;
}


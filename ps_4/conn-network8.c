/* 
 *    conn-network test program 8 
 *    
 *    tries creating a server on a busy port.   
 */

#include "defs.h"
#include "minithread.h"
#include "minisocket.h"
#include "synch.h"
#include "assert.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#define BUFFER_SIZE 100000
#define NUM_SERVER 32768
#define NUM_CLIENT 32768

int port = 80; /* port on which we do the communication */

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
  minisocket_t server1;

  server1 = minisocket_server_create(port, &error);
  assert(error == SOCKET_NOERROR); 
  minisocket_server_create(port, &error);
  printf("creating two servers on the same port cause a %s.\n", GetErrorDescription(error));  

  minisocket_close(server1);
  assert(error == SOCKET_NOERROR);
  return 0;
}

int client(int* arg) {
  network_address_t address;
  minisocket_error error;
 
  network_translate_hostname(hostname, address);
  
  /* create a network connection to the local machine */
  minisocket_client_create(address, port, &error);
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


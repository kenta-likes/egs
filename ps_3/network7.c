/**
 * network7.c
 * Have multiple threads listen on the same port
 * and check that they recieve packets in the same order as requested.
 * Also, check that if a thread doesn't receive, it blocks as expected.
 * Furthermore, checks that create_unbound returns the existing port if exists.
 *
 * */

#include "minithread.h"
#include "minimsg.h"
#include "synch.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define MAX_COUNT 100
#define BUFFER_SIZE 256

char* hostname;

int
receiveD(int* arg) {
  char buffer[BUFFER_SIZE];
  int length;
  miniport_t port;
  miniport_t from;

  minithread_yield();
  port = miniport_create_unbound(1);
  length = BUFFER_SIZE;
  minimsg_receive(port, &from, buffer, &length);
  printf("Hello from recieveD.\n");
  return 0;
 }

int
receiveC(int* arg) {
  char buffer[BUFFER_SIZE];
  int length;
  miniport_t port;
  miniport_t from;

  port = miniport_create_unbound(1);

  minithread_fork(receiveD, NULL);
  minithread_yield();
  length = BUFFER_SIZE;
  minimsg_receive(port, &from, buffer, &length);
  
  printf("receiveC got %s.\n", buffer);
  return 0;
}

int
receiveB(int* arg) {
  char buffer[BUFFER_SIZE];
  int length;
  miniport_t port;
  miniport_t from;

  port = miniport_create_unbound(1);

  minithread_fork(receiveC, NULL);
  minithread_yield();
  length = BUFFER_SIZE;
  minimsg_receive(port, &from, buffer, &length);
  printf("receiveB got %s.\n", buffer);
  return 0;
}

int
receiveA(int* arg) {
  char buffer[BUFFER_SIZE];
  int length;
  miniport_t port;
  miniport_t from;

  port = miniport_create_unbound(1);

  minithread_fork(receiveB, NULL);
  minithread_yield();
  length = BUFFER_SIZE;
  minimsg_receive(port, &from, buffer, &length);
  printf("receiveA got %s.\n", buffer);
  return 0;
}

int
transmit(int* arg) {
  char buffer[BUFFER_SIZE];
  int length;
  network_address_t addr;
  miniport_t port;
  miniport_t dest;

  AbortOnCondition(network_translate_hostname(hostname, addr) < 0,
                     "Could not resolve hostname, exiting.");

  port = miniport_create_unbound(0);
  dest = miniport_create_bound(addr, 1);

  printf("Sending packet 1\n");
  sprintf(buffer, "packet 1");
  length = strlen(buffer) + 1;
  minimsg_send(port, dest, buffer, length);

  printf("Sending packet 2\n");
  sprintf(buffer, "packet 2");
  minimsg_send(port, dest, buffer, length);

  printf("Sending packet 3\n");
  sprintf(buffer, "packet 3");
  minimsg_send(port, dest, buffer, length);
  
  return 0;
}

int
main(int argc, char** argv) {

  short fromport, toport;
  fromport = atoi(argv[1]);
  toport = atoi(argv[2]);
  network_udp_ports(fromport,toport); 

  if (argc > 3) {
    hostname = argv[3];
    minithread_system_initialize(transmit, NULL);
  }
  else {
    minithread_system_initialize(receiveA, NULL);
  }

  return -1;
}


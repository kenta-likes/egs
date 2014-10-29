/**
 * Send and receive a lot of packets and check whether system handles
 * without dropping, running out of memory or crashing.
 * */

#include "defs.h"
#include "minithread.h"
#include "minimsg.h"
#include "synch.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define BUFFER_SIZE 256
#define MAX_COUNT 10000

miniport_t port;

int
receive(int* arg) {
    char buffer[BUFFER_SIZE];
    int length;
    int i;
    miniport_t from;

    for (i=0; i<MAX_COUNT; i++) {
        length = BUFFER_SIZE;
        minimsg_receive(port, &from, buffer, &length);
        printf("%s", buffer);
        miniport_destroy(from);
    }

  return 0;
}

int
transmit(int* arg) {
    char buffer[BUFFER_SIZE];
    int length;
    int i;
    miniport_t write_port;
    network_address_t my_address;

    network_get_my_address(my_address);
    port = miniport_create_unbound(0);
    write_port = miniport_create_bound(my_address, 0);

    minithread_fork(receive, NULL);

    for (i=0; i<MAX_COUNT; i++) {
        printf("Sending packet %d.\n", i+1);
        sprintf(buffer, "Count is %d.\n", i+1);
        length = strlen(buffer) + 1;
        minimsg_send(port, write_port, buffer, length);
        minithread_sleep_with_timeout(10);
    }

    return 0;
}

int
main(int argc, char** argv) {
    short fromport;
    fromport = atoi(argv[1]);
    network_udp_ports(fromport,fromport); 
    minithread_system_initialize(transmit, NULL);
    return -1;
}


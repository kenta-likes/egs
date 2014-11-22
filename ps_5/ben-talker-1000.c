/* network test program 5
*/

#include "defs.h"
#include "minithread.h"
#include "minimsg.h"
#include "synch.h"
#include "read.h"
#include "read_private.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define BUFFER_SIZE 512

int receive(int* arg);

char* hostname;


int
receive(int* arg) {
    char buffer[BUFFER_SIZE];
    int length;
    miniport_t port;
    miniport_t from;
    
    miniterm_initialize();

    port = miniport_create_unbound(42);

    while(1){
        length = BUFFER_SIZE;
        memset(buffer, 0, BUFFER_SIZE);
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
    network_address_t addr;
    miniport_t port;
    miniport_t dest;

    AbortOnCondition(network_translate_hostname(hostname, addr) < 0,
                     "Could not resolve hostname, exiting.");

    port = miniport_create_unbound(42);
    dest = miniport_create_bound(addr, 42);

    minithread_fork(receive, NULL);

    while(1){
      memset(buffer, 0, BUFFER_SIZE);
      length = miniterm_read(buffer, BUFFER_SIZE);
      minimsg_send(port, dest, buffer, length);
    }

    return 0;
}

int
main(int argc, char** argv) {

    printf("Welcome to our lovely chat service\n");

    if (argc > 1) {
        hostname = argv[1];
        minithread_system_initialize(transmit, NULL);
    }
    else {
      printf("You fail. Enter the IP address of the computer you want to connect with.\n");
    }

    return -1;
}


/* network test program 1

   local loopback test: sends and then receives one message on the same machine.

   USAGE: ./network1 <port>

   where <port> is the minimsg port to use
*/

#include "minithread.h"
#include "minimsg.h"
#include "synch.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>


#define BUFFER_SIZE 256


miniport_t listen_port;
miniport_t send_port;

char text[] = "Hello, world!\n";
int textlen = 14;

int
thread(int* arg) {
    network_address_t my_address;
    int i;

    network_get_my_address(my_address);
    
    for (i = 0; i < 32768; i++){
      if (miniport_create_bound(my_address, 0)){
        //printf("Inside loop. Created bound port at i = %d\n", i);
      } else {
        printf("Inside loop. Failed bound port creation at i = %d\n", i);
      }
    }
    if ( miniport_create_bound(my_address, 0) ){
      //printf("Outside loop. Created bound port at i = %d\n", i);
    } else {
      printf("Outside loop. Failed bound port at i = %d\n", i);
    }

    return 0;
}

int
main(int argc, char** argv) {
    short fromport;
    fromport = atoi(argv[1]);
    network_udp_ports(fromport,fromport); 
    textlen = strlen(text) + 1;
    minithread_system_initialize(thread, NULL);
    return -1;
}



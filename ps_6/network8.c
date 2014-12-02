/* network test program 8
 * tests port creation functions.
  tests overflow in unbound ports and bound ports when calling
  create_unbound and create_bound.
  Unbound ports: "overflow" of unbound ports should just return
  port was assigned to a number gets returned
  Bound ports: "overflow" of unbound ports should result in 
  null return, since a port could not be found
*/

#include "minithread.h"
#include "minimsg.h"
#include "synch.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <limits.h>


#define BUFFER_SIZE 256


miniport_t listen_port;
miniport_t send_port;
static network_address_t broadcast_addr = {0};


int
thread(int* arg) {
    int i;
    //first try to assign invalid ports to unbound port
    for (i = 0; i < 100; i++){
      //try 100 random unbound port numbers less than 0
      listen_port = miniport_create_unbound(i-(rand()% INT_MAX/2));
      assert(listen_port == NULL);
    }
    for (i = 0; i < 100; i++){
      //try 100 random unbound port numbers greater than allowed
      //limit of MAX_PORT_NUM/2
      listen_port = miniport_create_unbound(MAX_PORT_NUM/2 + (rand() % MAX_PORT_NUM));
      assert(listen_port == NULL);
    }

    printf("Passed unbound port error case assignments.\n");

    //fill up all unbound/bound ports
    for (i = 0; i < MAX_PORT_NUM/2; i++){
      listen_port = miniport_create_unbound(i);
      send_port = miniport_create_bound(broadcast_addr, i);
      assert(listen_port != NULL);
      assert(send_port != NULL);
    }
    printf("Passed unbound/bound port assignments.\n");

    /*
     *try adding more ports to already full miniport array
     *unbound should succeed (return port alredy assigned)
     *but bound should fail (return null)
     *note: runtime is slow for this loop because
     *unbound port linearly checks an array for
     *unused slots.
    */
    for (i = 0; i < MAX_PORT_NUM/2; i++){
      listen_port = miniport_create_unbound(rand() % MAX_PORT_NUM/2);
      send_port = miniport_create_bound(broadcast_addr, i);
      assert(listen_port != NULL);
      assert(send_port == NULL);
      listen_port = NULL;
      send_port = NULL;
    }
    printf("Passed unbound/bound port overflow assignments.\n");

    return 0;
}

int
main(int argc, char** argv) {
    short fromport;
    fromport = atoi(argv[1]);
    network_udp_ports(fromport, fromport);
    minithread_system_initialize(thread, NULL);
    return -1;
}



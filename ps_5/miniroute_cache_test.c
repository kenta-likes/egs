/* test implementation of hash_table.
 */
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "miniroute_cache.h"
#include "miniroute.h"
#include "network.h"

int main(void) {
<<<<<<< HEAD
  //miniroute_cache_t cache;
  //cache = miniroute_cache_create();
=======
  miniroute_cache_t test_cache;
  network_address_t test_addr;
  miniroute_t test_route;

  test_cache = miniroute_cache_create();
  test_route = (miniroute_t)malloc(sizeof(struct miniroute));
  test_route->len = 1;
  test_route->route = (network_address_t*)malloc(sizeof(network_address_t));

  network_address_copy(test_addr,test_route->route[0]);

  miniroute_cache_put(test_cache, test_addr, test_route);
>>>>>>> fa8f0e32a605c4d45130e563e9c57d9fecd3c36d
  return 0;
}

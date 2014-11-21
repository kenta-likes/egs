/* test implementation of caches
 */
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "miniroute_cache.h"
#include "miniroute.h"
#include "network.h"
#include "minithread.h"

int test_cache (int* arg) {
  miniroute_cache_t test_cache;
  network_address_t test_key;
  miniroute_t test_route;
  miniroute_t tmp_route;
  int i;

  test_cache = miniroute_cache_create();

  //create a new route
  test_route = (miniroute_t)malloc(sizeof(struct miniroute));
  test_route->len = 1;
  test_route->route = (network_address_t*)malloc(sizeof(network_address_t));

  network_get_my_address(test_key);
  network_address_copy(test_key,test_route->route[0]);

  miniroute_cache_put(test_cache, test_key, test_route);
  tmp_route = miniroute_cache_get(test_cache, test_key);

  assert(tmp_route == test_route );
  assert(miniroute_cache_size(test_cache) == 1);
  printf("Add/get...........PASS\n");

  minithread_sleep_with_timeout(5000); //sleep for 5 seconds
  tmp_route = miniroute_cache_get(test_cache, test_key);

  assert(tmp_route == NULL);
  assert(miniroute_cache_size(test_cache) == 0);
  printf("Timeout removal...PASS\n");

  //add 20 random elements, check values entered correctly
  for (i = 0; i < SIZE_OF_ROUTE_CACHE; i++){
    network_get_my_address(test_key);
    test_key[0] = i; //make unique key for each i
    test_route = (miniroute_t)malloc(sizeof(struct miniroute)); //size 10 route
    test_route->route = (network_address_t*)malloc(sizeof(network_address_t) * 10);
    test_route->len = 10;
    network_address_copy(test_key, test_route->route[0]);
    network_address_copy(test_key, test_route->route[1]);
    network_address_copy(test_key, test_route->route[2]);
    network_address_copy(test_key, test_route->route[3]);
    network_address_copy(test_key, test_route->route[4]);
    network_address_copy(test_key, test_route->route[5]);
    network_address_copy(test_key, test_route->route[6]);
    network_address_copy(test_key, test_route->route[7]);
    network_address_copy(test_key, test_route->route[8]);
    network_address_copy(test_key, test_route->route[9]);

    miniroute_cache_put(test_cache, test_key, test_route);

    tmp_route = miniroute_cache_get(test_cache, test_key);
    //check cache entry and size
    assert(test_route == tmp_route);
    assert(miniroute_cache_size(test_cache) == i+1);
  }
  printf("20 entries......PASS\n");

  //add 5 more entries
  for (i = 0; i < 5; i++){
    //add 21st element for eviction, test
    network_get_my_address(test_key);
    test_key[0] = SIZE_OF_ROUTE_CACHE + i; //make another key
    test_route = (miniroute_t)malloc(sizeof(struct miniroute)); //size 10 route
    test_route->route = (network_address_t*)malloc(sizeof(network_address_t));
    test_route->len = 1;
    network_address_copy(test_key, test_route->route[0]);
    miniroute_cache_put(test_cache, test_key, test_route);
    tmp_route = miniroute_cache_get(test_cache, test_key);
    assert(test_route == tmp_route);
    assert(miniroute_cache_size(test_cache) == SIZE_OF_ROUTE_CACHE); //should still be 20 entries
  }

  //check first 5 entries created were evicted
  for (i = 0; i < 5; i++){
    network_get_my_address(test_key);
    test_key[0] = i; //try to get the first element again

    assert(miniroute_cache_get(test_cache, test_key) == NULL);
  }
  //check the 6th entry should not be evicted
  network_get_my_address(test_key);
  test_key[0] = 6; //try to get the first element again

  assert(miniroute_cache_get(test_cache, test_key) != NULL);

  printf("Eviction........PASS\n");

  minithread_sleep_with_timeout(5000); //sleep for 5 seconds
  tmp_route = miniroute_cache_get(test_cache, test_key);

  assert(tmp_route == NULL);
  assert(miniroute_cache_size(test_cache) == 0);
  printf("timeout 20......PASS\n");

  printf("Cache tests all PASS!!\n");
  return 0;
}


int
main(void) {
  minithread_system_initialize(test_cache, NULL);
  return 0;
}

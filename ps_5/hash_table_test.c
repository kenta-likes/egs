/* test implementation of hash_table.
 */
#include "stdlib.h"
#include "stdio.h"
#include "hash_table.h"
#include "assert.h"

int main(void) {
  hash_table_t ht;
  network_address_t addr1;
  network_address_t addr2;
  network_address_t addr3;
  network_address_t addr4;

  addr1[0] = 42;
  addr1[1] = 1;
  addr2[1] = 2;
  addr3[1] = 3;
  addr4[0] = 14;
  addr4[1] = 4;

  assert(network_compare_network_addresses(addr1, addr1));
  ht = hash_table_create();
  assert(hash_table_size(ht) == 0);
  assert(hash_table_capacity(ht) == 64);
  hash_table_add(ht, addr1, (void*)8);
  hash_table_add(ht, addr2, (void*)16);
  hash_table_add(ht, addr3, (void*)24);
  assert(hash_table_contains(ht, addr1));
  assert(!hash_table_contains(ht, addr4));
  assert((long)hash_table_get(ht, addr2) == 16);
  assert((long)hash_table_get(ht, addr3) == 24);
  assert(hash_table_size(ht) == 3);
  assert((long)hash_table_remove(ht, addr1) == 8);
  assert(!hash_table_contains(ht, addr1));
  assert(hash_table_capacity(ht) == 64);
  assert(hash_table_size(ht) == 2);
  assert(!hash_table_destroy(ht));
  
  ht = hash_table_create();
  assert(hash_table_size(ht) == 0);
  assert(!hash_table_add(ht, addr1, (void*)8));
  assert((long)hash_table_get(ht, addr2) == 0);
  assert(!hash_table_add(ht, addr2, (void*)16));
  assert((long)hash_table_get(ht, addr2) == 16);
  assert(!hash_table_destroy(ht));

  ht = hash_table_create();
  hash_table_add(ht, addr4, (void*)32);
  hash_table_add(ht, addr4, (void*)32);
  hash_table_add(ht, addr4, (void*)32);
  hash_table_add(ht, addr4, (void*)32);
  assert(hash_table_size(ht) == 4);
  assert(hash_table_contains(ht, addr4));
  assert((long)hash_table_remove(ht, addr4) == 32);
  assert((long)hash_table_remove(ht, addr1) == 0);
  assert(hash_table_contains(ht, addr4));
  assert(hash_table_size(ht) == 3);
  assert((long)hash_table_remove(ht, addr4) == 32);
  assert((long)hash_table_remove(ht, addr4) == 32);
  assert((long)hash_table_remove(ht, addr4) == 32);
  assert(!hash_table_contains(ht, addr4));
  assert(hash_table_size(ht) == 0);
  assert(!hash_table_destroy(ht));

  ht = hash_table_create();
  assert(hash_table_size(ht) == 0);
  assert(hash_table_capacity(ht) == 64);
  hash_table_add(ht, addr1, (void*)8);
  hash_table_add(ht, addr2, (void*)16);
  hash_table_add(ht, addr1, (void*)8);
  hash_table_add(ht, addr2, (void*)16);
  hash_table_add(ht, addr1, (void*)8);
  hash_table_add(ht, addr2, (void*)16);
  hash_table_add(ht, addr1, (void*)8);
  hash_table_add(ht, addr2, (void*)16);
  hash_table_add(ht, addr1, (void*)8);
  assert(hash_table_contains(ht, addr2));
  assert(!hash_table_contains(ht, addr4));
  assert(!hash_table_get(ht, addr4));
  hash_table_add(ht, addr2, (void*)16);
  hash_table_add(ht, addr1, (void*)8);
  hash_table_add(ht, addr2, (void*)16);
  hash_table_add(ht, addr1, (void*)8);
  hash_table_add(ht, addr2, (void*)16);
  hash_table_add(ht, addr1, (void*)8);
  hash_table_add(ht, addr2, (void*)16);
  hash_table_add(ht, addr1, (void*)8);
  hash_table_add(ht, addr2, (void*)16);
  hash_table_add(ht, addr1, (void*)8);
  hash_table_add(ht, addr2, (void*)16);
  hash_table_add(ht, addr1, (void*)8);
  hash_table_add(ht, addr2, (void*)16);
  hash_table_add(ht, addr1, (void*)8);
  hash_table_add(ht, addr2, (void*)16);
  hash_table_add(ht, addr1, (void*)8);
  hash_table_add(ht, addr2, (void*)16);
  hash_table_add(ht, addr1, (void*)8);
  hash_table_add(ht, addr2, (void*)16);
  hash_table_add(ht, addr1, (void*)8);
  hash_table_add(ht, addr2, (void*)16);
  hash_table_add(ht, addr1, (void*)8);
  hash_table_add(ht, addr2, (void*)16);
  hash_table_add(ht, addr1, (void*)8);
  hash_table_add(ht, addr2, (void*)16);
  hash_table_add(ht, addr1, (void*)8);
  hash_table_add(ht, addr2, (void*)16);
  hash_table_add(ht, addr1, (void*)8);
  hash_table_add(ht, addr2, (void*)16);
  hash_table_add(ht, addr1, (void*)8);
  hash_table_add(ht, addr2, (void*)16);
  hash_table_add(ht, addr1, (void*)8);
  hash_table_add(ht, addr2, (void*)16);
  hash_table_add(ht, addr1, (void*)8);
  hash_table_add(ht, addr2, (void*)16);
  hash_table_add(ht, addr1, (void*)8);
  hash_table_add(ht, addr2, (void*)16);
  hash_table_add(ht, addr1, (void*)8);
  hash_table_add(ht, addr2, (void*)16);
  hash_table_add(ht, addr1, (void*)8);
  hash_table_add(ht, addr2, (void*)16);
  assert(hash_table_size(ht) == 50);
  assert(hash_table_capacity(ht) == 128);
  assert((long)hash_table_remove(ht, addr1) == 8);
  assert((long)hash_table_remove(ht, addr2) == 16);
  assert((long)hash_table_remove(ht, addr1) == 8);
  assert((long)hash_table_remove(ht, addr2) == 16);
  assert((long)hash_table_remove(ht, addr1) == 8);
  assert((long)hash_table_remove(ht, addr2) == 16);
  assert((long)hash_table_remove(ht, addr1) == 8);
  assert((long)hash_table_remove(ht, addr2) == 16);
  assert((long)hash_table_remove(ht, addr1) == 8);
  assert((long)hash_table_remove(ht, addr2) == 16);
  assert((long)hash_table_remove(ht, addr1) == 8);
  assert((long)hash_table_remove(ht, addr2) == 16);
  assert((long)hash_table_remove(ht, addr1) == 8);
  assert((long)hash_table_remove(ht, addr2) == 16);
  assert((long)hash_table_remove(ht, addr1) == 8);
  assert((long)hash_table_remove(ht, addr2) == 16);
  assert((long)hash_table_remove(ht, addr1) == 8);
  assert((long)hash_table_remove(ht, addr2) == 16);
  assert((long)hash_table_remove(ht, addr1) == 8);
  assert((long)hash_table_remove(ht, addr2) == 16);
  assert(hash_table_size(ht) == 30);
  assert(hash_table_capacity(ht) == 64);
  printf("all tests pass\n");

  return 0;
}

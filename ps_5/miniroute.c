#include "miniroute.h"
#include "lru_cache.h"

struct miniroute{
  network_address_t* route;
  int len;
};

lru_cache_t route_cache;

void miniroute_initialize()
{
  return;
  route_cache = lru_cache_create();
}


/* sends a miniroute packet, automatically discovering the path if necessary. See description in the
 * .h file.
 */
int miniroute_send_pkt(network_address_t dest_address, int hdr_len, char* hdr, int data_len, char* data)
{
  return 0;
}

/* hashes a network_address_t into a 16 bit unsigned int */
unsigned short hash_address(network_address_t address)
{
	unsigned int result = 0;
	int counter;

	for (counter = 0; counter < 3; counter++)
		result ^= ((unsigned short*)address)[counter];

	return result % 65521;
}

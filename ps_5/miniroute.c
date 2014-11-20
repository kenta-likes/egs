#include "miniroute.h"
#include "miniroute_cache.h"

//our route cache
miniroute_cache_t route_cache;

void miniroute_initialize()
{
  route_cache = miniroute_cache_create();
  return;
}


/* sends a miniroute packet, automatically discovering the path if necessary. See description in the
 * .h file.
 */
int miniroute_send_pkt(network_address_t dest_address, int hdr_len, char* hdr, int data_len, char* data)
{
  //testing out stuf
  //miniroute_cache_add(dest_address);
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

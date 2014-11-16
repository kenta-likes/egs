/*
 * Generic bounded bhash table implementation.
 *
 */
#include "bhash_table.h"
#include <stdlib.h>
#include <stdio.h>

struct bhash_table {
  int dummy;
};

/* Return an empty bhash_table.  Returns NULL on error.
 * This table will have fixed capacity of capacity.
 */
bhash_table_t bhash_table_create(int capacity) {
  return NULL;
}

/* Adds a void* to a bhash_table (both specifed as parameters).
 * Returns 0 (success) or -1 (failure).
 */
int bhash_table_add(bhash_table_t ht, network_address_t key, void* value) {
  return -1;
}

/* Returns 0 if key in table or -1 if not.
 */
int bhash_table_contains(bhash_table_t ht, network_address_t key) {
  return -1;
}

/* Returns the value associated with the key or null if key not contained in table.
 */
void* bhash_table_get(bhash_table_t bhash_table, network_address_t key) {
  return NULL;
}

/* Free the bhash_table and return 0 (success) or -1 (failure).
 */
int bhash_table_destroy(bhash_table_t ht) {
  return -1;
}

/* Return the number of items in the bhash_table, or -1 if an error occured
 */
int bhash_table_size(bhash_table_t ht) {
  return -1;
}

/* Return the capacity of this bhash_table, or -1 if an error occured
 */
int bhash_table_capacity(bhash_table_t ht) {
  return -1;
}

/* Delete the first instance of key from the given bhash_table.
 * Returns the value associated with the key that was removed.
 */
void* bhash_table_remove(bhash_table_t bhash_table, network_address_t key) {
  return NULL;
}

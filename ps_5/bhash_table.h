/* Generic bhashtable manipulation functions
 * This table has fixed size.
 */
#ifndef __BOUNDED_HT_H__
#define __BOUNDED_HT_H__

#include "network.h"

/* bhash_table_t is a pointer to an internally maintained data structure.
 * Clients of this package do not need to know how bhash_tables are
 * represented.  They see and manipulate only bhash_table_t's.
 */
typedef struct bhash_table* bhash_table_t;

/* Return an empty bhash_table.  Returns NULL on error.
 * This table will have fixed capacity of capacity.
 */
extern bhash_table_t bhash_table_create(int capacity);

/* Adds a void* to a bhash_table (both specifed as parameters).
 * Returns 0 (success) or -1 (failure).
 * We don't allow NULL values.
 */
extern int bhash_table_add(bhash_table_t ht, network_address_t key, void* value);

/* Returns 0 if key in table or -1 if not.
 */
extern int bhash_table_contains(bhash_table_t ht, network_address_t key);

/* Free the bhash_table and return 0 (success) or -1 (failure).
 */
extern int bhash_table_destroy(bhash_table_t ht);

/* Return the number of items in the bhash_table, or -1 if an error occured
 */
extern int bhash_table_size(bhash_table_t ht);

/* Return the capacity of this bhash_table, or -1 if an error occured
 */
extern int bhash_table_capacity(bhash_table_t ht);

/* Delete the first instance of key from the given bhash_table.
 * Returns the value associated with the key that was removed.
 */
extern void* bhash_table_remove(bhash_table_t bhash_table, network_address_t key);

/* Returns the value associated with the key or null if key not contained in table.
 */
extern void* bhash_table_get(bhash_table_t bhash_table, network_address_t key);

#endif /*__BOUNDED_HT_H__*/

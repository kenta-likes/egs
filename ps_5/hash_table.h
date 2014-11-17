/* Generic hashtable manipulation functions
 * This table is thread safe.
 */
#ifndef __BOUNDED_HT_H__
#define __BOUNDED_HT_H__

#include "network.h"

/* hash_table_t is a pointer to an internally maintained data structure.
 * Clients of this package do not need to know how hash_tables are
 * represented.  They see and manipulate only hash_table_t's.
 */
typedef struct hash_table* hash_table_t;

/* Return an empty hash_table.  Returns NULL on error.
 * This table will have fixed capacity of capacity.
 */
extern hash_table_t hash_table_create(int capacity);

/* Adds a void* to a hash_table (both specifed as parameters).
 * Returns 0 (success) or -1 (failure).
 * We don't allow NULL vals.
 */
extern int hash_table_add(hash_table_t ht, network_address_t key, void* val);

/* Returns 1 if key in table or 0 if not.
 */
extern int hash_table_contains(hash_table_t ht, network_address_t key);

/* Free the hash_table and return 0 (success) or -1 (failure).
 */
extern int hash_table_destroy(hash_table_t ht);

/* Return the number of items in the hash_table, or -1 if an error occured
 */
extern int hash_table_size(hash_table_t ht);

/* Return the capacity of this hash_table, or -1 if an error occured
 */
extern int hash_table_capacity(hash_table_t ht);

/* Delete the first instance of key from the given hash_table.
 * Returns the val associated with the key that was removed.
 */
extern void* hash_table_remove(hash_table_t ht, network_address_t key);

/* Returns the val associated with the key or null if key not contained in table.
 */
extern void* hash_table_get(hash_table_t ht, network_address_t key);

#endif /*__BOUNDED_HT_H__*/

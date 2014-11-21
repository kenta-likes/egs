/* Generic bounded hash table implementation.
 */
#include "hash_table.h"
#include <stdlib.h>
#include <stdio.h>
#include "miniroute.h"
#include "interrupts.h"

#define INITIAL_CAPACITY 64
 
typedef struct ht_node {
  network_address_t key;
  void* val;
  struct ht_node* next;
}* ht_node_t;

struct hash_table {
  ht_node_t* array;
  int size;
  int capacity;
  int threshold_grow;
  int threshold_shrink;
};

/* Return an empty hash_table.  Returns NULL on error.
 */
hash_table_t hash_table_create() {
  hash_table_t new_ht;
  int i;

  new_ht = (hash_table_t)malloc(sizeof(struct hash_table));
  if (new_ht == NULL) {
    return NULL;
  }

  new_ht->size = 0;
  new_ht->capacity = INITIAL_CAPACITY;
  new_ht->threshold_grow = INITIAL_CAPACITY * 3 / 4;
  new_ht->threshold_shrink = INITIAL_CAPACITY / 4; 

  new_ht->array = (ht_node_t*)malloc(sizeof(ht_node_t) * new_ht->capacity);
  if (new_ht->array == NULL) {
    free(new_ht);
    return NULL;
  }

  for (i = 0; i < new_ht->capacity; i++) {
    new_ht->array[i] = NULL;
  }

  return new_ht;  
}

/* helper function for hash_table_add and hash_table_remove.
 * Returns 0 on success, -1 on failure.
 */
int hash_table_resize(hash_table_t ht, int grow) {
  int old_capacity;
  ht_node_t* old_array;
  ht_node_t curr;
  ht_node_t temp;
  unsigned short idx;
  int i;

  printf("entering resize\n");
  old_capacity = ht->capacity;
  old_array = ht->array;

  if (grow) {
    ht->capacity *= 2;
    ht->threshold_grow *= 2;
    ht->threshold_shrink *= 2;

    ht->array = (ht_node_t*)malloc(sizeof(ht_node_t) * ht->capacity);
    if (ht->array == NULL) {
      ht->capacity /= 2;
      ht->threshold_grow /= 2;
      ht->threshold_shrink /= 2;
      ht->array = old_array;
      return -1;
    }
  }
  else {
    ht->capacity /= 2;
    ht->threshold_grow /= 2;
    ht->threshold_shrink /= 2;

    ht->array = (ht_node_t*)malloc(sizeof(ht_node_t) * ht->capacity);
    if (ht->array == NULL) {
      ht->capacity *= 2;
      ht->threshold_grow *= 2;
      ht->threshold_shrink *= 2;
      ht->array = old_array;
      return -1;
    }
  }

  for (i = 0; i < ht->capacity; i++) {
    ht->array[i] = NULL;
  }
  // move things and free
  for (i = 0; i < old_capacity; i++) {
    if (old_array[i]) {
      curr = old_array[i];
      while (curr != NULL) {
        temp = curr;
        curr = curr->next;
        idx = hash_address(temp->key) % ht->capacity;
        if (ht->array[idx]) {
          temp->next = ht->array[idx];
          ht->array[idx] = temp;
        }
        else {
          temp->next = NULL;
          ht->array[idx] = temp;
        }
      } 
    }
    // nothing at this idx, move on
  }
  free(old_array);
  return 0;
}

/* Adds a void* to a hash_table (both specifed as parameters).
 * Returns 0 (success) or -1 (failure).
 * We don't allow NULL vals.
 */
int hash_table_add(hash_table_t ht, network_address_t key, void* val) {
  unsigned short idx;
  ht_node_t new_node;
  
  if (ht == NULL || val == NULL) {
    return -1;
  }

  if (ht->size + 1 > ht->threshold_grow) {
    if (hash_table_resize(ht, 1)) { // error
      return -1;
    } 
  }

  new_node = (ht_node_t)malloc(sizeof(struct ht_node));
  if (new_node == NULL) {
    return -1;
  }
  
  network_address_copy(key, new_node->key);
  new_node->val = val;
 
  idx = hash_address(key) % ht->capacity;
  if (ht->array[idx]) {
    new_node->next = ht->array[idx];
    ht->array[idx] = new_node;
  }
  else {
    ht->array[idx] = new_node;
  }
  
  ht->size++;
  return 0;
}

/* Returns 1 if key in table or 0 if not.
 */
int hash_table_contains(hash_table_t ht, network_address_t key) {
  unsigned short idx;
  ht_node_t curr;
  
  if (ht == NULL) {
    return 0;
  }

  idx = hash_address(key) % ht->capacity;
  if (ht->array[idx]) {
    curr = ht->array[idx];
    while (curr != NULL) {
      if (network_compare_network_addresses(curr->key, key)) { // same 
        return 1;
      }
      else {
        curr = curr->next;
      }
    }  
  }
  return 0;
}

/* Returns the val associated with the key or null if key not contained in table.
 */
void* hash_table_get(hash_table_t ht, network_address_t key) {
  unsigned short idx;
  ht_node_t curr;
  
  if (ht == NULL) {
    return NULL;
  }

  idx = hash_address(key) % ht->capacity;
  if (ht->array[idx]) {
    curr = ht->array[idx];
    while (curr != NULL) {
      if (network_compare_network_addresses(curr->key, key)) { // same 
        return curr->val;
      }
      else {
        curr = curr->next;
      }
    }  
    return NULL;
  }
  else {
    return NULL;
  }
}

/* Free the hash_table and return 0 (success) or -1 (failure).
 */
int hash_table_destroy(hash_table_t ht) {
  int i;
  int array_len;
  ht_node_t curr;
  ht_node_t temp;

  printf("entering hash_table_destroy\n");
  if (ht == NULL) {
    printf("exiting hash_table_destroy on FAILURE\n");
    return -1;
  }
  printf("capacity: %d\n", ht->capacity);
  printf("size: %d\n", ht->size);
  array_len = ht->capacity;
  for (i = 0; i < array_len; i++) {
    printf("in for loop, %dth iteration of\n", i);
    if (ht->array[i]) {
      printf("removing entries at idx %d", i);
      curr = ht->array[i];
      while (curr != NULL) {
        temp = curr;
        curr = curr->next;
        free(temp);
      } 
    }
  }
  printf("exit for loop\n");
  free(ht->array);
  free(ht);
  
  printf("exiting hash_table_destroy on SUCCESS\n");
  return 0;
}

/* Return the number of items in the hash_table, or -1 if an error occured
 */
int hash_table_size(hash_table_t ht) {
  if (ht == NULL) {
    return -1;
  }

  return ht->size;
}

/* Return the capacity of of the array backing this hash_table, 
 * or -1 if an error occured
 */
int hash_table_capacity(hash_table_t ht) {
  if (ht == NULL) {
    return -1;
  }

  return ht->capacity;
}

/* Delete the first instance of key from the given hash_table.
 * Returns the val associated with the key that was removed.
 */
void* hash_table_remove(hash_table_t ht, network_address_t key) {
  unsigned short idx;
  ht_node_t curr;
  ht_node_t temp;
  void* val;
  
  if (ht == NULL) {
    return NULL;
  }

  if (ht->capacity > INITIAL_CAPACITY && ht->size - 1 < ht->threshold_shrink) {
    if (hash_table_resize(ht, 0)) { // error
      return NULL;
    }
  }

  idx = hash_address(key) % ht->capacity;
  if (ht->array[idx]) {
    curr = ht->array[idx];
    if (network_compare_network_addresses(curr->key, key)) { // found it
      val = curr->val;
      ht->array[idx] = curr->next;
      free(curr);
      ht->size--; 
      return val;
    }
    while (curr->next != NULL) {
      if (network_compare_network_addresses(curr->next->key, key)) { // same 
        val = curr->next->val;
        temp = curr->next;
        curr->next = curr->next->next;
        free(temp);
        ht->size--; 
        return val;
      }
      else {
        curr = curr->next;
      }
    }
    return NULL;
  }
  else {
    return NULL;
  }
}


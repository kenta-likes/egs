/*
 * Generic bounded bhash table implementation.
 *
 */
#include "bhash_table.h"
#include <stdlib.h>
#include <stdio.h>
#include "miniroute.h"

typedef struct ht_node {
  network_address_t key;
  void* value;
  struct ht_node* next;
}* ht_node_t;

struct bhash_table {
  ht_node_t* array;
  int size;
  int capacity;
};

/* Return an empty bhash_table.  Returns NULL on error.
 * This table will have fixed capacity of capacity.
 */
bhash_table_t bhash_table_create(int capacity) {
  bhash_table_t new_ht;
  int i;

  if (capacity < 1) {
    return NULL;
  }

  new_ht = (bhash_table_t)malloc(sizeof(struct bhash_table));
  if (new_ht == NULL) {
    return NULL;
  }

  new_ht->size = 0;
  new_ht->capacity = capacity;

  new_ht->array = (ht_node_t*)malloc(sizeof(ht_node_t) * capacity * 2);
  if (new_ht->array == NULL) {
    free(new_ht);
    return NULL;
  }

  for (i = 0; i < (new_ht->capacity * 2); i++) {
    new_ht->array[i] = NULL;
  }

  return new_ht;  
}

/* Adds a void* to a bhash_table (both specifed as parameters).
 * Returns 0 (success) or -1 (failure).
 * We don't allow NULL values.
 */
int bhash_table_add(bhash_table_t ht, network_address_t key, void* value) {
  unsigned short idx;
  ht_node_t new_node;
  
  if (ht == NULL || ht->size == ht->capacity || value == NULL) {
    return -1;
  }
  
  new_node = (ht_node_t)malloc(sizeof(struct ht_node));
  if (new_node == NULL) {
    return -1;
  }
  
  network_address_copy(key, new_node->key);
  new_node->value = value;
 
  idx = hash_address(key) % (ht->capacity * 2);
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
int bhash_table_contains(bhash_table_t ht, network_address_t key) {
  unsigned short idx;
  ht_node_t curr;
  
  if (ht == NULL) {
    return 0;
  }

  idx = hash_address(key) % (ht->capacity * 2);
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

/* Returns the value associated with the key or null if key not contained in table.
 */
void* bhash_table_get(bhash_table_t ht, network_address_t key) {
  unsigned short idx;
  ht_node_t curr;
  
  if (ht == NULL) {
    return NULL;
  }

  idx = hash_address(key) % (ht->capacity * 2);
  if (ht->array[idx]) {
    curr = ht->array[idx];
    while (curr != NULL) {
      if (network_compare_network_addresses(curr->key, key)) { // same 
        return curr->value;
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

/* Free the bhash_table and return 0 (success) or -1 (failure).
 */
int bhash_table_destroy(bhash_table_t ht) {
  int i;
  int array_len;
  ht_node_t curr;
  ht_node_t temp;

  if (ht == NULL) {
    return -1;
  }

  array_len = ht->capacity * 2;
  for (i = 0; i < array_len; i++) {
    if (ht->array[i]) {
      curr = ht->array[i];
      while (curr != NULL) {
        temp = curr;
        curr = curr->next;
        free(temp);
      } 
    }
  }
  free(ht->array);
  free(ht);
  
  return 0;
}

/* Return the number of items in the bhash_table, or -1 if an error occured
 */
int bhash_table_size(bhash_table_t ht) {
  if (ht == NULL) {
    return -1;
  }

  return ht->size;
}

/* Return the capacity of this bhash_table, or -1 if an error occured
 */
int bhash_table_capacity(bhash_table_t ht) {
  if (ht == NULL) {
    return -1;
  }

  return ht->capacity;
}

/* Delete the first instance of key from the given bhash_table.
 * Returns the value associated with the key that was removed.
 */
void* bhash_table_remove(bhash_table_t ht, network_address_t key) {
  unsigned short idx;
  ht_node_t curr;
  ht_node_t temp;
  void* value;
  
  if (ht == NULL) {
    return NULL;
  }

  idx = hash_address(key) % (ht->capacity * 2);
  if (ht->array[idx]) {
    curr = ht->array[idx];
    if (network_compare_network_addresses(curr->key, key)) { // found it
      value = curr->value;
      ht->array[idx] = curr->next;
      free(curr);
      ht->size--; 
      return value;
    }
    while (curr->next != NULL) {
      if (network_compare_network_addresses(curr->next->key, key)) { // same 
        value = curr->next->value;
        temp = curr->next;
        curr->next = curr->next->next;
        free(temp);
        ht->size--; 
        return value;
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

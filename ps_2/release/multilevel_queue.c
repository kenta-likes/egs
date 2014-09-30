/* * Multilevel queue manipulation functions  */
#include "multilevel_queue.h" 
#include <stdlib.h> 
#include <stdio.h> 

typedef struct node { 
  queue_t queue; 
  struct node* next;
} node;

typedef node* node_t;

typedef struct multilevel_queue {
  node* head;
  int count;
  int num_levels;
} multilevel_queue; 

/*
 * Returns an empty multilevel queue with number_of_levels levels. On error should return NULL.
 */
multilevel_queue_t multilevel_queue_new(int number_of_levels)
{
  int i = 0;
  queue_t new_q = NULL;
  node_t new_node = NULL;
  multilevel_queue_t new_multi_q = NULL;
  node_t tail = NULL;
  
  new_multi_q = (multilevel_queue_t)malloc(sizeof(multilevel_queue));
  
  // check for error
  if (new_multi_q == NULL) return NULL;

  new_multi_q->num_levels = number_of_levels;
  new_multi_q->count = 0;
  new_multi_q->head = NULL;

  for (i = 0; i < number_of_levels; i++) {
    new_node = (node_t)malloc(sizeof(node));
    new_q = queue_new();
    
    // check for error
    if (new_node == NULL || new_q == NULL) return NULL;
    
    // check if currently no levels
    if (new_multi_q->head == NULL) {
      new_node->queue = new_q;
      new_node->next = new_node;
      new_multi_q->head = new_node;
    }
    // otherwise are levels
    else {
      tail->next = new_node;
      new_node->next = new_multi_q->head;
    }
    tail = new_node; 
  }

  return new_multi_q;
}

/*
 * Appends an void* to the multilevel queue at the specified level. Return 0 (success) or -1 (failure).
 */
int multilevel_queue_enqueue(multilevel_queue_t multi_q, int level, void* item)
{ 
  node_t curr_node = NULL;
  int curr_level = 0;

  // check for errors
  if (multi_q == NULL) return -1;
  if (level < 0 || level >= multi_q->num_levels) return -1;
  
  // we know this multi_q has at least one level
  curr_node = multi_q->head;
  while (curr_level < level) curr_node = curr_node->next;
  
  // enqueue at correct level and check for error
  if (queue_append(curr_node->queue,item) == -1) return -1;
  
  multi_q->count++;
  return 0;
}

/*
 * Dequeue and return the first void* from the multilevel queue starting at the specified level. 
 * Levels wrap around so as long as there is something in the multilevel queue an item should be returned.
 * Return the level that the item was located on and that item if the multilevel queue is nonempty,
 * or -1 (failure) and NULL if queue is empty.
 */
int multilevel_queue_dequeue(multilevel_queue_t multi_q, int level, void** item)
{ 
  int i = 0;
  node_t curr_node = NULL;
  
  // check if queue empty
  if (multi_q->count == 0) {
    *item = NULL;
    return -1;
  }

  // multi_q must have at least one level, because contains elements
  curr_node = multi_q->head;

  for (i = 0; i < level; i++) curr_node = curr_node->next;

  for (i = 0; i < multi_q->num_levels; i++) {
    // no items at this level
    if (queue_length(curr_node->queue) == 0) continue;
    
    // failed dequeue
    else if (queue_dequeue(curr_node->queue,item) == -1) {
      return -1;
    }

    // successful dequeue
    else {
      multi_q->count--;
      return level;
    }
    level = (level + 1) % multi_q->num_levels;
    curr_node = curr_node->next;
  }

  return -1;
}

/* 
 * Free the queue and return 0 (success) or -1 (failure). Do not free the queue nodes; this is
 * the responsibility of the programmer.
 */
int multilevel_queue_free(multilevel_queue_t multi_q)
{
  int i = 0;
  node_t curr = NULL;
  node_t temp = NULL;

  if (multi_q == NULL) return -1;
  
  curr = multi_q->head;
  for (i = 0; i < multi_q->num_levels; i++) {
    if (queue_free(curr->queue) == -1) return -1;
    
    temp = curr;
    curr = curr->next;
    free(curr);
  }
  
  free(multi_q);  
  return 0;
}

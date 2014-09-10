/*
 * Generic queue implementation.
 *
 */
#include "queue.h"
#include <stdlib.h>
#include <stdio.h>

typedef struct node_t{
  void* val;
  struct node_t* next;
} node_t;

typedef struct queue{
  int len;
  struct node_t* head;
  struct node_t* tail;
} queue;

/*
 * Return an empty queue.
 */
queue_t
queue_new() {
  queue_t new_q = (queue_t)malloc(sizeof(queue));
  if (new_q == NULL){
    return NULL;
  }
  new_q->len = 0;
  new_q->head = NULL;
  new_q->tail = NULL;
  return new_q;
}

/*
 * Prepend a void* to a queue (both specifed as parameters).  Return
 * 0 (success) or -1 (failure).
 */
int
queue_prepend(queue_t queue, void* item) {
  node_t* new_node;
  if (queue == NULL){
    return -1;
  }
  new_node = (node_t*)malloc(sizeof(node_t));
  if (new_node == NULL){
    return -1;
  }
  new_node->next = queue->head;
  new_node->val = item;
  if (queue->head == NULL) {
    queue->tail = new_node;
  }
  queue->head = new_node;
  queue->len++;
  return 0;
}

/*
 * Append a void* to a queue (both specifed as parameters). Return
 * 0 (success) or -1 (failure).
 */
int
queue_append(queue_t queue, void* item) {
  node_t* new_node;
  if (queue == NULL){
    return -1;
  }
  new_node = (node_t*)malloc(sizeof(node_t));
  if (new_node == NULL){
    return -1;
  }
  new_node->next = NULL;
  new_node->val = item;
  if (queue->head == NULL){
    queue->head = new_node;
    queue->tail = new_node;
  } else {
    queue->tail->next = new_node;
    queue->tail = new_node;
  }
  queue->len++;
  return 0;
}

/*
 * Dequeue and return the first void* from the queue or NULL if queue
 * is empty.  Return 0 (success) or -1 (failure).
 */
int
queue_dequeue(queue_t queue, void** item) {
  node_t* temp;
  if (queue == NULL || queue->len == 0){
    return -1;
  }
  *item = queue->head->val;
  if (queue->len == 1){
    free(queue->head);
    queue->head = NULL;
    queue->tail = NULL;
  } else {
    temp = queue->head;
    queue->head = queue->head->next;
    free(temp);
  }
  queue->len--;
  return 0;
}

/*
 * Iterate the function parameter over each element in the queue.  The
 * additional void* argument is passed to the function as its first
 * argument and the queue element is the second.  Return 0 (success)
 * or -1 (failure).
 */
int
queue_iterate(queue_t queue, func_t f, void* item) {
    return 0;
}

/*
 * Free the queue and return 0 (success) or -1 (failure).
 */
int
queue_free (queue_t queue) {
  if (queue == NULL || queue->len != 0){
    return -1;
  }
  free(queue);
  return 0;
}

/*
 * Return the number of items in the queue.
 */
int
queue_length(queue_t queue) {
    return queue->len;
}


/*
 * Delete the specified item from the given queue.
 * Return -1 on error.
 */
int
queue_delete(queue_t queue, void* item) {
    return 0;
}

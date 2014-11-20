/* Performs any initialization of the miniroute layer, if required. */
#include "miniroute_cache.h"
#include "interrupts.h"
#include "miniroute.h"

#define CACHE_TIME 3000.0f //3000 millisconds = 3 seconds.

//node type for storing addresses
typedef struct dlink_node{
  struct dlink_node* next;
  struct dlink_node* prev;
  network_address_t key; //making specific data type to avoid casting void*'s
}* dlink_node_t;

//doubly linked list type
struct dlink_list{
  dlink_node_t hd;
  dlink_node_t tl;
  int len;
};

struct miniroute_cache{
  hash_table_t cache_table;
  struct dlink_list cache_list;
};

typedef struct cache_entry{
  miniroute_t route;
  alarm_t route_alarm;
}* cache_t;

struct miniroute{
  network_address_t route;
  int len;
};


miniroute_cache_t miniroute_cache_create(){
  miniroute_cache_t route_cache;

  route_cache = (miniroute_cache_t)malloc(sizeof(struct miniroute_cache));
  if (!route_cache){
    return NULL;
  }
  route_cache->cache_list.hd = NULL;
  route_cache->cache_list.tl = NULL;
  route_cache->cache_list.len = 0;
  route_cache->cache_table = hash_table_create();
  return route_cache;
}

/*destroys an entry from the hashtable*/
int destroy_entry(hash_table_t route_table, network_address_t key){
  //free struct stored
  //call hashtable remove to remove linked list node
  return 0;
}


/*
 * adds a route to the route cache
 * If route already exists in cache, this function
 * simply updates the alarm so that it will not be
 * evicted after another 3 seconds.
 * If the route did not already exist in the cache,
 * the oldest route is evicted and the new route is added
 * */
int miniroute_cache_put(miniroute_cache_t route_cache, network_address_t key, miniroute_t val){
  interrupt_level_t l;
  dlink_node_t tmp;

  l = set_interrupt_level(DISABLED);
  if (route_cache->cache_list.len >= SIZE_OF_ROUTE_CACHE) {
    //evict oldest cache
    destroy_entry(route_cache->cache_table, route_cache->cache_list.tl->key);//destroy key value
    tmp = route_cache->cache_list.tl->prev; //store new tail
    free(route_cache->cache_list.tl); //free list node
    tmp->next = NULL; //set last node next to null
    route_cache->cache_list.tl = tmp; //set new tail
    (route_cache->cache_list.len)--; //reduce len
  }
  //add the new entry
  hash_table_add(route_cache->cache_table, key, (void*)val);
  tmp = (dlink_node_t)malloc(sizeof(struct dlink_node));
  network_address_copy(tmp->key, key);
  tmp->next = route_cache->cache_list.hd;
  tmp->prev = NULL;
  route_cache->cache_list.hd->prev = tmp;
  route_cache->cache_list.hd = tmp;
  set_interrupt_level(l);

  return 0;
}


/*
 * returns the route associated with a given address,
 * if it exists in the cache. If not, it will return NULL
 * Updates the alarm if called and exists
 * */
miniroute_t miniroute_cache_get(miniroute_cache_t route_cache, network_address_t key){
  return NULL;
}

/* Performs any initialization of the miniroute layer, if required. */
#include "miniroute_cache.h"
#include "interrupts.h"
#include "minithread.h"

#define CACHE_TIME 30.0f //3 seconds = 100 milliseconds(cycle time) * 30

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

//cache data type
struct miniroute_cache{
  hash_table_t cache_table;
  struct dlink_list cache_list;
};

//hash table entries
typedef struct cache_entry{
  miniroute_t route;
  alarm_t route_alarm;
}* cache_entry_t;

//alarm argument
typedef struct cache_alarm_arg{
  miniroute_cache_t route_cache;
  dlink_node_t node;
}* cache_alarm_arg_t;

//forward declaration for alarm function
void destroy_entry(void* arg);


/**
 *  Initializes a new route cache
 *  Creates a new empty hash table, an empty
 *  linked list, and returns a reference to the cache
 *  object
 */
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

int miniroute_cache_size(miniroute_cache_t route_cache){
  return route_cache->cache_list.len;
}

/* destroys an entry from the hashtable,
 * removes the node in the cache's list
 * with it.*/
void destroy_entry(void* arg){
  cache_alarm_arg_t entry_alarm;
  dlink_node_t delete_node;
  cache_entry_t delete_entry;

  entry_alarm = (cache_alarm_arg_t)arg; //alarm info stored
  delete_node = entry_alarm->node; //node to be removed from cache list
  delete_entry = hash_table_get(entry_alarm->route_cache->cache_table, delete_node->key);

  //remove entry from hashtable. If element doesn't exist, function is safe
  hash_table_remove(entry_alarm->route_cache->cache_table, delete_node->key);


  if (delete_node->next == NULL || delete_node->prev == NULL){
    //check if this is a single node list
    if (entry_alarm->route_cache->cache_list.len == 1){
      free(delete_node);
      free(delete_entry->route->route);
      free(delete_entry->route);
      free(delete_entry);
      entry_alarm->route_cache->cache_list.hd = NULL;
      entry_alarm->route_cache->cache_list.tl = NULL;
      (entry_alarm->route_cache->cache_list.len)--;
      //free(entry_alarm);
    }
    else { //remove head or tail
      if (delete_node->next == NULL){ //tail
        delete_node->prev->next = NULL;
        entry_alarm->route_cache->cache_list.tl = delete_node->prev;
        free(delete_node);
        free(delete_entry->route->route);
        free(delete_entry->route);
        free(delete_entry);
        (entry_alarm->route_cache->cache_list.len)--;
        //free(entry_alarm);
      }
      else { //head
        delete_node->next->prev = NULL;
        entry_alarm->route_cache->cache_list.hd = delete_node->next;
        free(delete_node);
        free(delete_entry->route->route);
        free(delete_entry->route);
        free(delete_entry);
        (entry_alarm->route_cache->cache_list.len)--;
        //free(entry_alarm);
      }
    }
  }
  else { //node is somewhere in middle of list
    delete_node->prev->next = delete_node->next;
    delete_node->next->prev = delete_node->prev;
    free(delete_node);
    free(delete_entry->route->route);
    free(delete_entry->route);
    free(delete_entry);
    (entry_alarm->route_cache->cache_list.len)--;
    //free(entry_alarm);
  }
  return;
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
  cache_entry_t new_entry;
  cache_alarm_arg_t new_alarm;
  cache_alarm_arg_t evict_alarm;

  if (route_cache == NULL || val == NULL) {
    return -1;
  }

  printf("entering miniroute_cache_put\n");
  l = set_interrupt_level(DISABLED);

  //if cache full, remove entry from hashtable and list and deregister alarm
  if (route_cache->cache_list.len >= SIZE_OF_ROUTE_CACHE) {
    //deregister alarm. If alarm was already executed, then this function is a no-op
    deregister_alarm( ((cache_entry_t)hash_table_get(route_cache->cache_table, route_cache->cache_list.tl->key))->route_alarm);
    evict_alarm = (cache_alarm_arg_t)malloc(sizeof(struct cache_alarm_arg));
    evict_alarm->route_cache = route_cache; //route_cache
    evict_alarm->node = route_cache->cache_list.tl; //save node info to alarm arg
    destroy_entry((void*)evict_alarm);
    evict_alarm = NULL;
  }
  new_alarm = NULL;

  //create new entry
  new_entry = (cache_entry_t)malloc(sizeof(struct cache_entry));
  if (!new_entry){
    set_interrupt_level(l);
    printf("exiting miniroute_cache_put on FAILURE\n");
    return -1;
  }
  //create new node
  tmp = NULL;
  tmp = (dlink_node_t)malloc(sizeof(struct dlink_node));
  if (!tmp){
    free(new_entry);
    set_interrupt_level(l);
    printf("exiting miniroute_cache_put on FAILURE\n");
    return -1; //error catch
  }

  //create new alarm arg
  new_alarm = (cache_alarm_arg_t)malloc(sizeof(struct cache_alarm_arg));
  if (!new_alarm){
    free(new_entry);
    free(tmp);
    set_interrupt_level(l);
    printf("exiting miniroute_cache_put on FAILURE\n");
    return -1;
  }


  network_address_copy(key, tmp->key);
  tmp->next = route_cache->cache_list.hd; //will be null if list was empty
  tmp->prev = NULL;
  if (route_cache->cache_list.len != 0){
    route_cache->cache_list.hd->prev = tmp;
    route_cache->cache_list.hd = tmp;
  }
  else {
    route_cache->cache_list.hd = tmp;
    route_cache->cache_list.tl = tmp;
  }
  (route_cache->cache_list.len)++;

  new_alarm->route_cache = route_cache; //route_cache
  new_alarm->node = tmp; //save node info to arg

  //register the alarm
  new_entry->route_alarm = set_alarm(CACHE_TIME, destroy_entry, (void*)new_alarm, minithread_time());
  if (!(new_entry->route_alarm)){
    free(new_entry);
    free(tmp);
    free(new_alarm);
    set_interrupt_level(l);
    printf("exiting miniroute_cache_put on FAILURE\n");
    return -1;
  }
  new_entry->route = val;
  //add the entry to the hash table
  hash_table_add(route_cache->cache_table, key, (void*)new_entry);

  set_interrupt_level(l);

  printf("exiting miniroute_cache_put on SUCCESS\n");
  return 0;
}


/*
 * returns the route associated with a given address,
 * if it exists in the cache. If not, it will return NULL
 * */
miniroute_t miniroute_cache_get(miniroute_cache_t route_cache, network_address_t key){
  cache_entry_t entry;

  printf("entering miniroute_cache_get\n");
  if (route_cache == NULL) {
    printf("exiting miniroute_cache_get on FAILURE\n");
    return NULL;
  }

  entry = hash_table_get(route_cache->cache_table, key);
  if (entry == NULL){
    printf("exiting miniroute_cache_get on FAILURE\n");
    return NULL;
  }
  else {
    printf("exiting miniroute_cache_get on SUCCESS\n");
    return entry->route;
  }
  
}

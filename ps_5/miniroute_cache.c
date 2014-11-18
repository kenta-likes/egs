/* Performs any initialization of the miniroute layer, if required. */
#include "miniroute_cache.h"

#define CACHE_TIME = 3000.0f //3000 millisconds = 3 seconds.

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

struct miniroute{
  network_address_t* route;
  int len;
}

//container for route cache
miniroute_cache_t route_cache;

miniroute_cache miniroute_cache_create(){
  route_cache.miniroute_listtt.hd = NULL;
  route_cache.miniroute_list.hd = NULL;
  route_cache.miniroute_list.tl = NULL;
  route_cache.miniroute_list.len = 0;
  route_cache.miniroute_table = hash_table_create();
}

int miniroute_add( network_address_t key, miniroute_t val){
  if (hash_table_contains(route_cache.miniroute_table, new_key)){
    //keep element but update the alarm
    deregister_alarm(hash_table_get(route_cache.miniroute_table, new_key).destroy_alarm);
  }
    if (route_cache.num_elems < SIZE_OF_ROUTE_CACHE) {
      hash_table_add(route_cache.miniroute_table, new_key, );
    }
    return 0;

  return -1;

}

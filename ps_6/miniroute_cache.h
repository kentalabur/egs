#include <stdlib.h>
#include <stdio.h>
#include "alarm.h"
#include "network.h"
#include "hash_table.h"
#include "miniroute.h"

/*This class is interrupt safe since it
 * touches the alarms which deal with
 * clock handlers. Both functions disable
 * interrupts
 * */


/*route_cache data type*/
typedef struct miniroute_cache* miniroute_cache_t;

/*initializer function for route cache*/
miniroute_cache_t miniroute_cache_create();

/*destructor function*/
void miniroute_cache_destroy(miniroute_cache_t route_cache);


/*
 * returns number of elements
 * in this cache
 * */
int miniroute_cache_size(miniroute_cache_t route_cache);

/*
 * returns the route associated with a given address,
 * if it exists in the cache. If not, it will return NULL
 * Updates the alarm if called and exists
 * */
miniroute_t miniroute_cache_get(miniroute_cache_t route_cache, network_address_t key);

/*
 * adds a route to the route cache
 * If route already exists in cache, this function
 * simply updates the alarm so that it will not be
 * evicted after another 3 seconds.
 * If the route did not already exist in the cache,
 * the LRU route is evicted and the new route is added
 * */
int miniroute_cache_put(miniroute_cache_t route_cache, network_address_t key, miniroute_t val);

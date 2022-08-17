#ifndef TTL_MAP_H
#define TTL_MAP_H

#include <pthread.h>
#include "hashmap.h"
#include "timewheel.h"

typedef struct ttlmap {
	struct hashmap	*hmap;
	size_t		elsize;
	pthread_mutex_t	hlock;
	timewheel_t	*tw;
} ttlmap;

ttlmap *ttlmap_new(size_t elsize, size_t cap, 
                            uint64_t seed0, uint64_t seed1,
                            uint64_t (*hash)(const void *item, 
                                             uint64_t seed0, uint64_t seed1),
                            int (*compare)(const void *a, const void *b, 
                                           void *udata),
                            void (*elfree)(void *item),
                            void *udata, 
			    timewheel_t *twptr);

struct ttlmap *ttlmap_new_with_allocator(
                            void *(*malloc)(size_t), 
                            void *(*realloc)(void *, size_t), 
                            void (*free)(void*),
                            size_t elsize, size_t cap, 
                            uint64_t seed0, uint64_t seed1,
                            uint64_t (*hash)(const void *item, 
                                             uint64_t seed0, uint64_t seed1),
                            int (*compare)(const void *a, const void *b, 
                                           void *udata),
                            void (*elfree)(void *item),
                            void *udata, 
			    timewheel_t *twptr);
void ttlmap_free(ttlmap *map);
void ttlmap_clear(ttlmap *map, bool update_cap);
size_t ttlmap_count(ttlmap *map);
bool ttlmap_oom(ttlmap *map);
void *ttlmap_get(ttlmap *map, const void *item);
void *ttlmap_set(ttlmap *map, const void *item, int ttl_ms);
void *ttlmap_delete(ttlmap *map, void *item);
void *ttlmap_probe(ttlmap *map, uint64_t position);
bool ttlmap_scan(ttlmap *map,
                  bool (*iter)(const void *item, void *udata), void *udata);
bool ttlmap_iter(ttlmap *map, size_t *i, void **item);

uint64_t ttlmap_sip(const void *data, size_t len, 
                     uint64_t seed0, uint64_t seed1);
uint64_t ttlmap_murmur(const void *data, size_t len, 
                        uint64_t seed0, uint64_t seed1);

#endif
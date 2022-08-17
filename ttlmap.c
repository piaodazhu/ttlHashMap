#include "ttlmap.h"

ttlmap *ttlmap_new(size_t elsize, size_t cap, 
                            uint64_t seed0, uint64_t seed1,
                            uint64_t (*hash)(const void *item, 
                                             uint64_t seed0, uint64_t seed1),
                            int (*compare)(const void *a, const void *b, 
                                           void *udata),
                            void (*elfree)(void *item),
                            void *udata,
			    timewheel_t *twptr)
{
	ttlmap *map = (ttlmap*)malloc(sizeof(ttlmap));
	map->hmap = hashmap_new(elsize, cap, seed0, seed1, hash, compare, elfree, udata);
	map->elsize = elsize;
	if (twptr == NULL) {
		map->tw = tw_new();
		tw_runthread(map->tw);
	} else {
		pthread_mutex_lock(&twptr->ref_lock);
		twptr->ref_count++;
		pthread_mutex_unlock(&twptr->ref_lock);
		map->tw = twptr;
	}
	pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;
	memcpy(&map->hlock, &init_mutex, sizeof(init_mutex));
	return map;
}

ttlmap *ttlmap_new_with_allocator(
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
			    timewheel_t *twptr)
{
	ttlmap *map = (ttlmap*)malloc(sizeof(ttlmap));
	map->hmap = hashmap_new_with_allocator(malloc, realloc, free, elsize, cap, seed0, seed1, hash, compare, elfree, udata);
	if (twptr == NULL) {
		map->tw = tw_new();
		tw_runthread(map->tw);
	} else {
		pthread_mutex_lock(&twptr->ref_lock);
		twptr->ref_count++;
		pthread_mutex_unlock(&twptr->ref_lock);
		map->tw = twptr;
	}
	pthread_mutex_t init_mutex = PTHREAD_MUTEX_INITIALIZER;
	memcpy(&map->hlock, &init_mutex, sizeof(init_mutex));
	return map;
}


void ttlmap_free(ttlmap *map)
{
	hashmap_free(map->hmap);
	pthread_mutex_destroy(&map->hlock);
	
	int shouldbefree = 0;
	pthread_mutex_lock(&map->tw->ref_lock);
	if (--map->tw->ref_count == 0) {
		shouldbefree = 1;
	}
	pthread_mutex_unlock(&map->tw->ref_lock);
	if (shouldbefree) {
		tw_free(map->tw);
	}
	free(map);
}


void ttlmap_clear(ttlmap *map, bool update_cap)
{
	pthread_mutex_lock(&map->hlock);
	hashmap_clear(map->hmap, update_cap);
	pthread_mutex_unlock(&map->hlock);
}

size_t ttlmap_count(ttlmap *map)
{
	size_t ret;
	pthread_mutex_lock(&map->hlock);
	hashmap_count(map->hmap);
	pthread_mutex_unlock(&map->hlock);
}


bool ttlmap_oom(ttlmap *map)
{
	bool ret;
	pthread_mutex_lock(&map->hlock);
	ret = hashmap_oom(map->hmap);
	pthread_mutex_unlock(&map->hlock);
	return ret;
}


void *ttlmap_get(ttlmap *map, const void *item)
{
	void *ret;
	pthread_mutex_lock(&map->hlock);
	ret = hashmap_get(map->hmap, item);
	pthread_mutex_unlock(&map->hlock);
	return ret;
}

struct _delitemarg {
	ttlmap *map;
	void *item;
};
void _deleteitem(void *arg) {
	// printf("call delete\n");
	struct _delitemarg *darg = arg;
	ttlmap_delete(darg->map, darg->item);
	free(darg->item);
	free(darg);
}

void *ttlmap_set(ttlmap *map, const void *item, int ttl_ms)
{
	void *ret;
	pthread_mutex_lock(&map->hlock);
	ret = hashmap_set(map->hmap, item);
	pthread_mutex_unlock(&map->hlock);
	if (ttl_ms > 0) {
		struct _delitemarg *darg = (struct _delitemarg*)malloc(sizeof(struct _delitemarg));
		darg->item = malloc(map->elsize);
		memcpy(darg->item, item, map->elsize);
		darg->map = map;
		tw_addtask(map->tw, ttl_ms, _deleteitem, darg);
	}
	return ret;
}


void *ttlmap_delete(ttlmap *map, void *item)
{
	void *ret;
	pthread_mutex_lock(&map->hlock);
	ret = hashmap_delete(map->hmap, item);
	pthread_mutex_unlock(&map->hlock);
	return ret;
}


void *ttlmap_probe(ttlmap *map, uint64_t position)
{
	void *ret;
	pthread_mutex_lock(&map->hlock);
	ret = hashmap_probe(map->hmap, position);
	pthread_mutex_unlock(&map->hlock);
	return ret;
}


bool ttlmap_scan(ttlmap *map,
                  bool (*iter)(const void *item, void *udata), void *udata)
{
	bool ret;
	pthread_mutex_lock(&map->hlock);
	ret = hashmap_scan(map->hmap, iter, udata);
	pthread_mutex_unlock(&map->hlock);
	return ret;
}


bool ttlmap_iter(ttlmap *map, size_t *i, void **item)
{
	bool ret;
	pthread_mutex_lock(&map->hlock);
	ret = hashmap_iter(map->hmap, i, item);
	pthread_mutex_unlock(&map->hlock);
	return ret;
}

uint64_t ttlmap_sip(const void *data, size_t len, 
                     uint64_t seed0, uint64_t seed1)
{
	return hashmap_sip(data, len, seed0, seed1);
}


uint64_t ttlmap_murmur(const void *data, size_t len, 
                        uint64_t seed0, uint64_t seed1)
{
	return hashmap_murmur(data, len, seed0, seed1);
}

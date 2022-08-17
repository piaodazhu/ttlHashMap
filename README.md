# ttlHashMap
A general-purpose, thread-safe hash map that supports TTL of the items. It is built upon https://github.com/tidwall/hashmap.c.git .

## Features
- All features from tidwall/hashmap.c
- The TTL of item can be set for expiration
- Thread-Safety
- A general-purpose task scheduler implemented with time wheel and can be reused by maintaining refcount

## Example
```c
#include <stdio.h>
#include <string.h>
#include "ttlmap.h"

struct user
{
	char *name;
	int age;
};

int user_compare(const void *a, const void *b, void *udata)
{
	const struct user *ua = a;
	const struct user *ub = b;
	return strcmp(ua->name, ub->name);
}

bool user_iter(const void *item, void *udata)
{
	const struct user *user = item;
	printf("%s (age=%d)\n", user->name, user->age);
	return true;
}

uint64_t user_hash(const void *item, uint64_t seed0, uint64_t seed1)
{
	const struct user *user = item;
	return ttlmap_sip(user->name, strlen(user->name), seed0, seed1);
}

int main()
{
	// create a new hash map where each item is a `struct user`. The second
	// argument is the initial capacity. The third and fourth arguments are
	// optional seeds that are passed to the following hash function.
	ttlmap *map = ttlmap_new(sizeof(struct user), 0, 0, 0,
				 user_hash, user_compare, NULL, NULL, NULL);

	// Here we'll load some users into the hash map. Each set operation
	// performs a copy of the data that is pointed to in the second argument.
	ttlmap_set(map, &(struct user){.name = "Dale", .age = 44}, 3000);
	ttlmap_set(map, &(struct user){.name = "Roger", .age = 68}, 6000);
	ttlmap_set(map, &(struct user){.name = "Jane", .age = 47}, 9000);

	struct user *user;

	printf("\n-- get some users --\n");
	user = ttlmap_get(map, &(struct user){.name = "Jane"});
	printf("%s age=%d\n", user->name, user->age);

	user = ttlmap_get(map, &(struct user){.name = "Roger"});
	printf("%s age=%d\n", user->name, user->age);

	user = ttlmap_get(map, &(struct user){.name = "Dale"});
	printf("%s age=%d\n", user->name, user->age);

	user = ttlmap_get(map, &(struct user){.name = "Tom"});
	printf("%s\n", user ? "exists" : "not exists");

	printf("\n-- iterate over all users (ttlmap_scan) --\n");
	ttlmap_scan(map, user_iter, NULL);

	printf("\n-- iterate over all users (ttlmap_iter) --\n");
	size_t iter = 0;
	void *item;
	while (ttlmap_iter(map, &iter, &item))
	{
		const struct user *user = item;
		printf("%s (age=%d)\n", user->name, user->age);
	}

	sleep(4);
	printf("\n-- iterate over all users (after 4s) --\n");
	ttlmap_scan(map, user_iter, NULL);
	
	sleep(3);
	printf("\n-- iterate over all users (after 7s) --\n");
	ttlmap_scan(map, user_iter, NULL);
	
	sleep(3);
	printf("\n-- iterate over all users (after 10s) --\n");
	ttlmap_scan(map, user_iter, NULL);

	ttlmap_free(map);
	return 0;
}

// make:
// gcc ttlmaptest.c ttlmap.c timewheel.c hashmap.c -lpthread && ./a.out

// output:
// -- get some users --
// Jane age=47
// Roger age=68
// Dale age=44
// not exists

// -- iterate over all users (ttlmap_scan) --
// Dale (age=44)
// Roger (age=68)
// Jane (age=47)

// -- iterate over all users (ttlmap_iter) --
// Dale (age=44)
// Roger (age=68)
// Jane (age=47)

// -- iterate over all users (after 4s) --
// Roger (age=68)
// Jane (age=47)

// -- iterate over all users (after 7s) --
// Jane (age=47)

// -- iterate over all users (after 10s) --
// }
```

## Functions
### Basic
```sh
ttlmap_new      # allocate a new ttl hash map
ttlmap_free     # free the ttl hash map
ttlmap_count    # returns the number of items in the ttl hash map
ttlmap_set      # insert or replace an existing item and return the previous
ttlmap_get      # get an existing item (ttl will be set if ttl_ms > 0)
ttlmap_delete   # delete and return an item
ttlmap_clear    # clear the ttl hash map
```
### Iteration
```sh
ttlmap_iter     # loop based iteration over all items in ttl hash map 
ttlmap_scan     # callback based iteration over all items in ttl hash map
```
### Hash helpers
```sh
ttlmap_sip      # returns hash value for data using SipHash-2-4
ttlmap_murmur   # returns hash value for data using MurmurHash3
```
## License
ttlHashMap source code is available under the MIT License.
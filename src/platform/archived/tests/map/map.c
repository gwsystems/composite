#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <assert.h>

#define LINUX_TEST
#include <cmap.h>

#define NTESTS 4096

CMAP_CREATE_STATIC(static_map);

struct pair {
	long id;
	int *val;
};

int
in_pairs(struct pair *ps, int len, long id)
{
	for (; len >= 0; len--) {
		if (ps[len].id == id) return 1;
	}
	return 0;
}

/* I separate this out so that we can easily confirm that the compiler
 * is doing the proper optimizations. */
void *
do_lookups(struct pair *ps, cmap_t *v)
{
	return cmap_lookup(v, ps->id);
}

int
main(void)
{
	struct pair pairs[NTESTS * 2];
	int         i;
	cmap_t *    dyn_map;

	dyn_map = cmap_alloc();
	assert(dyn_map->data);
	cmap_init_static(&static_map);
	assert(static_map.data);
	for (i = 0; i < NTESTS; i++) {
		int nid;

		pairs[i].val = malloc(sizeof(int));
		nid          = cmap_add(dyn_map, pairs[i].val);
		assert(nid == cmap_add(&static_map, pairs[i].val));
		pairs[i].id     = nid;
		*(pairs[i].val) = nid;
	}
	for (i = 0; i < NTESTS; i++) {
		assert(do_lookups(&pairs[i], dyn_map) == pairs[i].val);
		assert(do_lookups(&pairs[i], &static_map) == pairs[i].val);
	}
	assert(cmap_lookup(dyn_map, 2047) == pairs[2047].val);
	//	assert(!cmap_del(dyn_map, 2046));
	//	assert(cmap_lookup(dyn_map, 2047) == pairs[2047].val);

	for (i = 0; i < NTESTS / 2; i++) {
		assert(!cmap_del(dyn_map, pairs[i].id));
		assert(!cmap_del(&static_map, pairs[i].id));
	}
	for (i = 0; i < NTESTS / 2; i++) {
		int nid;

		pairs[i].val = malloc(sizeof(int));
		nid          = cmap_add(dyn_map, pairs[i].val);
		assert(nid == cmap_add(&static_map, pairs[i].val));
		*pairs[i].val = nid;
	}
	for (i = NTESTS; i < NTESTS * 2; i++) {
		int nid;

		pairs[i].val = malloc(sizeof(int));
		nid          = cmap_add(dyn_map, pairs[i].val);
		assert(nid == cmap_add(&static_map, pairs[i].val));
		pairs[i].id     = nid;
		*(pairs[i].val) = nid;
	}
	for (i = 0; i < NTESTS * 2; i++) {
		assert(!cmap_del(dyn_map, pairs[i].id));
		assert(!cmap_del(&static_map, pairs[i].id));
	}
	cmap_free(dyn_map);

	return 0;
}

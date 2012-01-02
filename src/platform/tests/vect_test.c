#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <assert.h>

#define COS_LINUX_ENV
#include "composite/src/components/include/cos_vect2.h"

#define NTESTS 1024
#define RANGE  (1<<16)

COS_VECT_CREATE_STATIC(static_vect);

struct pair {
	long id;
	void *val;
};

int in_pairs(struct pair *ps, int len, long id)
{
	for (; len >= 0 ; len--) {
		if (ps[len].id == id) return 1;
	}
	return 0;
}

void *do_lookups(struct pair *ps, cos_vect_t *v)
{
	return cos_vect_lookup(v, ps->id);
}

int main(void)
{
	struct pair pairs[NTESTS];
	int i;
	cos_vect_t *dyn_vect;

	dyn_vect = cos_vect_alloc();
	assert(dyn_vect);
	cos_vect_init(dyn_vect);
	cos_vect_init_static(&static_vect);

	for (i = 0 ; i < NTESTS ; i++) {
		do {
			pairs[i].id = rand() % RANGE;
		} while (in_pairs(pairs, i-1, pairs[i].id));
		pairs[i].val = malloc(10);
		assert(!cos_vect_add_id(dyn_vect, pairs[i].val, pairs[i].id));
		assert(!cos_vect_add_id(&static_vect, pairs[i].val, pairs[i].id));
	}
	for (i = 0 ; i < NTESTS ; i++) {
		assert(do_lookups(&pairs[i], dyn_vect) == pairs[i].val);
		assert(do_lookups(&pairs[i], &static_vect) == pairs[i].val);
	}
	for (i = 0 ; i < NTESTS ; i++) {
		assert(!cos_vect_del(dyn_vect, pairs[i].id));
		assert(!cos_vect_del(&static_vect, pairs[i].id));
	}
	cos_vect_free(dyn_vect);
	
	return 0;
}

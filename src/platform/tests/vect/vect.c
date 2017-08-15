#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <assert.h>

#define LINUX_TEST
#include <cvect.h>

#define NTESTS 1024
#define RANGE (1 << 16)

CVECT_CREATE_STATIC(static_vect);

struct pair {
	long  id;
	void *val;
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
do_lookups(struct pair *ps, cvect_t *v)
{
	return cvect_lookup(v, ps->id);
}

int
main(void)
{
	struct pair pairs[NTESTS];
	int         i;
	cvect_t *   dyn_vect;

	dyn_vect = cvect_alloc();
	assert(dyn_vect);
	cvect_init(dyn_vect);
	cvect_init_static(&static_vect);

	for (i = 0; i < NTESTS; i++) {
		do {
			pairs[i].id = rand() % RANGE;
		} while (in_pairs(pairs, i - 1, pairs[i].id));
		pairs[i].val = malloc(10);
		assert(!cvect_add_id(dyn_vect, pairs[i].val, pairs[i].id));
		assert(!cvect_add_id(&static_vect, pairs[i].val, pairs[i].id));
	}
	for (i = 0; i < NTESTS; i++) {
		assert(do_lookups(&pairs[i], dyn_vect) == pairs[i].val);
		assert(do_lookups(&pairs[i], &static_vect) == pairs[i].val);
	}
	for (i = 0; i < NTESTS; i++) {
		assert(!cvect_del(dyn_vect, pairs[i].id));
		assert(!cvect_del(&static_vect, pairs[i].id));
	}
	cvect_free(dyn_vect);

	return 0;
}

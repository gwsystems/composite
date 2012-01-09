#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <assert.h>

#define LINUX_TEST
#include <cvect_compressed.h>

#define NTESTS 1024
#define RANGE  (1<<16)

struct cvectc static_vect;

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

/* I separate this out so that we can easily confirm that the compiler
 * is doing the proper optimizations. */
void *do_lookups(struct pair *ps, struct cvectc *v)
{
	return cvectc_lookup(v, ps->id);
}

int main(void)
{
	struct pair pairs[NTESTS];
	int i;

	assert(__cvectc_prefix_match(0xF0F00000, 0xF00F0000, 8));
	assert(!__cvectc_prefix_match(0xF0F00000, 0xF00F0000, 9));

	cvectc_init(&static_vect);

	cvectc_add(&static_vect, (void*)1, 1);
	cvectc_add(&static_vect, (void*)2, 2);
	cvectc_add(&static_vect, (void*)3, 3);
	cvectc_add(&static_vect, (void*)4, 4);

	printf("%p %p %p %p\n",
	       cvectc_lookup(&static_vect, 1),
	       cvectc_lookup(&static_vect, 2),
	       cvectc_lookup(&static_vect, 3),
	       cvectc_lookup(&static_vect, 4));

	

	/* for (i = 0 ; i < NTESTS ; i++) { */
	/* 	do { */
	/* 		pairs[i].id = rand() % RANGE; */
	/* 	} while (in_pairs(pairs, i-1, pairs[i].id)); */
	/* 	pairs[i].val = malloc(10); */
	/* 	assert(!cvect_add_id(&static_vect, pairs[i].val, pairs[i].id)); */
	/* } */
	/* for (i = 0 ; i < NTESTS ; i++) { */
	/* 	assert(do_lookups(&pairs[i], &static_vect) == pairs[i].val); */
	/* } */
	/* for (i = 0 ; i < NTESTS ; i++) { */
	/* 	assert(!cvect_del(&static_vect, pairs[i].id)); */
	/* } */
	
	return 0;
}

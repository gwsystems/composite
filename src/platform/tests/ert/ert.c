#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <assert.h>

typedef unsigned short int u16_t;
typedef unsigned int u32_t;
#define unlikely(x)     __builtin_expect(!!(x), 0)
#define LINUX_TEST
#include <ertrie.h>

#define NTESTS 1024
#define RANGE  (1<<16)

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

#include <sys/mman.h>
static void *
unit_allocfn(void *d, int sz, int leaf) 
{ 
	(void)d; (void)leaf;
	return mmap(NULL, sz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
}

static void 
unit_freefn(void *d, void *m, int sz, int leaf) 
{ 
	(void)d; (void)leaf;
	munmap(m, sz);
}
ERT_CREATE_DEF(unit, 2, 9, 10, sizeof(int*), unit_allocfn, unit_freefn)

/* I separate this out so that we can easily confirm that the compiler
 * is doing the proper optimizations. */
void *
do_lookups(struct pair *ps, struct ert *v)
{
	return unit_lkupp(v, ps->id);
}

void *
do_addr_lookups(struct pair *ps, struct ert *v)
{
	int *p = unit_lkup(v, ps->id);
	return (void*)*p;
}

int
do_add(struct pair *ps, struct ert *v, void *d)
{
	return unit_add(v, ps->id, d);
}

int main(void)
{
	struct pair pairs[NTESTS];
	int i;
	struct ert *dyn_vect;

	dyn_vect = unit_alloc();
	assert(dyn_vect);

//	assert(unit_add(dyn_vect, (void*)134537224, 35336));
//	assert(unit_lookup(dyn_vect, 35336) == (void*)134537224);
//	printf("\n");

	for (i = 0 ; i < NTESTS ; i++) {
		do {
			pairs[i].id = rand() % RANGE;
		} while (in_pairs(pairs, i-1, pairs[i].id));
		pairs[i].val = malloc(10);
//		printf(">>> adding %x, %p\n", pairs[i].id, pairs[i].val);
		assert(!unit_add(dyn_vect, pairs[i].id, pairs[i].val));
//		printf(">>> vect %p, inserted %p\n", unit_lkup(dyn_vect, pairs[i].id), pairs[i].val);
//		printf("%x: %x, %x\n", pairs[i].id, do_addr_lookups(&pairs[i], dyn_vect), pairs[i].val);
		assert(unit_lkupp(dyn_vect, pairs[i].id) == pairs[i].val);

//		printf("\n");
	}
	for (i = 0 ; i < NTESTS ; i++) {
//		printf("%x: %d, %d\n", pairs[i].id, do_addr_lookups(&pairs[i], dyn_vect), pairs[i].val);
		assert(do_lookups(&pairs[i], dyn_vect) == pairs[i].val);
		assert(do_addr_lookups(&pairs[i], dyn_vect) == pairs[i].val);
	}
	for (i = 0 ; i < NTESTS ; i++) {
		assert(!unit_del(dyn_vect, pairs[i].id));
	}
	unit_free(dyn_vect);
	
	return 0;
}

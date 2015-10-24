#include <stdio.h>
#include <ps_slab.h>

#define SMALLSZ 1
#define LARGESZ 8000

struct small {
	char x[SMALLSZ];
};

struct larger {
	char x[LARGESZ];
};

PS_SLAB_CREATE_DEF(s, sizeof(struct small))
PS_SLAB_CREATE(l, sizeof(struct larger), PS_PAGE_SIZE * 128, 1)

#define ITER       (1024)
#define SMALLCHUNK 2
#define LARGECHUNK 16

void
mark(char *c, int sz, char val)
{
	int i;

	for (i = 0 ; i < sz ; i++) c[i] = val;
}

void
chk(char *c, int sz, char val)
{
	int i;

	for (i = 0 ; i < sz ; i++) assert(c[i] == val);
}

struct small  *s[ITER];
struct larger *l[ITER];

int
main(void)
{
	int i, j;
	unsigned long long start, end;

	printf("Slabs:\n"
	       "\tobjsz %lu, objmem %lu, nobj %lu\n"
	       "\tobjsz %lu, objmem %lu, nobj %lu\n",
	       sizeof(struct small),  ps_slab_objmem_s(), ps_slab_nobjs_s(),
	       sizeof(struct larger), ps_slab_objmem_l(), ps_slab_nobjs_l());

	start = ps_tsc();
	for (j = 0 ; j < ITER ; j++) {
		for (i = 0 ; i < LARGECHUNK ; i++) s[i] = ps_slab_alloc_l();
		for (i = 0 ; i < LARGECHUNK ; i++) ps_slab_free_l(s[i]);
	}
	end = ps_tsc();
	end = (end-start)/(ITER*LARGECHUNK);
	printf("Average cost of large slab alloc+free: %lld\n", end);

	ps_slab_alloc_s();
	start = ps_tsc();
	for (j = 0 ; j < ITER ; j++) {
		for (i = 0 ; i < SMALLCHUNK ; i++) s[i] = ps_slab_alloc_s();
		for (i = 0 ; i < SMALLCHUNK ; i++) ps_slab_free_s(s[i]);
	}
	end = ps_tsc();
	end = (end-start)/(ITER*SMALLCHUNK);
	printf("Average cost of small slab alloc+free: %lld\n", end);

	printf("Starting mark & check for increasing numbers of allocations.\n");
	for (i = 0 ; i < ITER ; i++) {
		l[i] = ps_slab_alloc_l();
		mark(l[i]->x, sizeof(struct larger), i);
		for (j = i+1 ; j < ITER ; j++) {
			l[j] = ps_slab_alloc_l();
			mark(l[j]->x, sizeof(struct larger), j);
		}
		for (j = i+1 ; j < ITER ; j++) {
			chk(l[j]->x, sizeof(struct larger), j);
			ps_slab_free_l(l[j]);
		}
	}
	for (i = 0 ; i < ITER ; i++) {
		assert(l[i]);
		chk(l[i]->x, sizeof(struct larger), i);
		ps_slab_free_l(l[i]);
	}
	return 0;
}


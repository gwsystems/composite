#include <stdio.h>
#define LINUX_TEST
#include <cslab.h>

struct small {
	int x;
};

struct larger {
	char x[88];
};

CSLAB_CREATE(s, sizeof(struct small));
CSLAB_CREATE(l, sizeof(struct larger));

#define ITER 1024

void
mark(char *c, int sz, char val)
{
	int i;

	for (i = 0; i < sz; i++) {
		c[i] = val;
	}
}

void
chk(char *c, int sz, char val)
{
	int i;

	for (i = 0; i < sz; i++) {
		assert(c[i] == val);
	}
}

#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A"(val))

int
main(void)
{
	struct small *     s[ITER];
	struct larger *    l[ITER];
	int                i, j;
	unsigned long long start, end;

	printf("small per slab %d, large %d\n", slab_s_max_objs, slab_l_max_objs);
	l[0] = cslab_alloc_l();
	rdtscll(start);
	for (j = 0; j < ITER; j++) {
		for (i = 0; i < 10; i++) {
			s[i] = cslab_alloc_l();
		}
		for (i = 0; i < 10; i++) {
			cslab_free_l(s[i]);
		}
	}
	rdtscll(end);
	end = (end - start) / (ITER * 10);
	printf("Average cost of slab alloc+free: %lld\n", end);

	for (i = 0; i < ITER; i++) {
		s[i] = cslab_alloc_s();
		l[i] = cslab_alloc_l();
		mark(l[i]->x, sizeof(struct larger), i);
		for (j = 0; j < i / 2; j++) {
			cslab_free_s(s[j]);
			chk(l[j]->x, sizeof(struct larger), j);
			cslab_free_l(l[j]);
		}
		for (j = 0; j < i / 2; j++) {
			s[j] = cslab_alloc_s();
			l[j] = cslab_alloc_l();
			mark(l[j]->x, sizeof(struct larger), j);
		}
	}
	for (i = 0; i < ITER; i++) {
		cslab_free_s(s[i]);
		chk(l[i]->x, sizeof(struct larger), i);
		cslab_free_l(l[i]);
	}
	return 0;
}

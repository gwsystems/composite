#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <assert.h>

#define LINUX_TEST
#define CVECTC_STATS
#include <cvect_compressed.h>

#define NTESTS (1 << 13)
#define RANGE (1 << 30)
#define OUTPUT (1 << 10)

struct cvectc static_vect;
long          ids[NTESTS];
void          print(struct cvectc *s);

int
in_pairs(int nid)
{
	int i;

	if (nid == 0) return 1;
	for (i = 0; i < NTESTS; i++) {
		if (ids[i] == (long)nid) return 1;
	}
	return 0;
}

static void
__s(struct cvcentry *e, int size, int depth, int *tot, int *n, int *max, int *etot, int *en, int *emax)
{
	int i;

	if (!__cvc_isleaf(e)) {
		*etot += size;
		(*en)++;
		if (size > *emax) *emax = size;
	}
	for (i = 0; i < size; i++) {
		struct cvcentry *new = &e[i];

		if (__cvc_isleaf(new) && __cvc_ispresent(__cvc_leaf(new))) {
			*tot += depth;
			(*n)++;
			if (*max < depth) *max = depth;
		} else if (!__cvc_isleaf(new)) {
			struct cvcdir *d = __cvc_dir(new);
			assert(d->next);
			__s(d->next, __cvectc_size(d), depth + 1, tot, n, max, etot, en, emax);
		}
	}
}

static void
ps(void)
{
	int tot, n, max, etot, en, emax;

	tot = n = max = etot = en = emax = 0;
	__s(static_vect.d.e.d.next, __cvectc_size(__cvc_dir(&static_vect.d)), 1, &tot, &n, &max, &etot, &en, &emax);
	if (!n)
		printf("No data\n");
	else
		printf("Average depth %d, max %d; average level size %d, max %d\n", tot / n, max, etot / en, emax);
}

#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A"(val))

void
rand_test(void)
{
	int                i, j;
	unsigned long long start, end;

	printf("Incremental tests:\n");
	/* Incremental identifiers between 0 and NTESTS (best case) */
	for (i = 1; i < NTESTS; i++) {
		assert(!cvectc_add(&static_vect, (void *)i, i));
		//		print(&static_vect);
		if (((i + 1) % OUTPUT) == 0) {
			cvectc_stats();
			ps();
		}
	}
	cvectc_stats();
	rdtscll(start);
	for (i = 1; i < NTESTS; i++) {
		assert(cvectc_lookup(&static_vect, i) == (void *)i);
	}
	rdtscll(end);
	printf("lookup cost: %lld\n", (end - start) / (NTESTS - 1));
	for (i = 1; i < NTESTS; i++) {
		cvectc_del(&static_vect, i);
		if (((i + 1) % OUTPUT) == 0) {
			cvectc_stats();
			ps();
		}
	}

	printf("\nRandom tests:\n");
	/* Random identifiers between 0 and RANGE (2^16) (worst case) */
	for (j = 0; j < 2; j++) {
		cvectc_debug = 1;
		for (i = 0; i < NTESTS; i++) {
			int id;
			do {
				id = rand() % RANGE;
			} while (in_pairs(id));
			ids[i] = id;
			printf("%x ", id);
			assert(!cvectc_add(&static_vect, (void *)id, id));
			if (((i + 1) % OUTPUT) == 0) {
				cvectc_stats();
				ps();
			}
		}

		rdtscll(start);
		for (i = 0; i < NTESTS; i++) {
			assert(cvectc_lookup(&static_vect, ids[i]) == (void *)ids[i]);
		}
		rdtscll(end);
		cvectc_stats();
		printf("lookup cost: %lld\n", (end - start) / NTESTS);
		for (i = 0; i < NTESTS; i++) {
			cvectc_del(&static_vect, ids[i]);
			if (((i + 1) % OUTPUT) == 0) {
				cvectc_stats();
				ps();
			}
		}
		for (i = 0; i < NTESTS; i++) {
			assert((void *)CVECTC_INIT_VAL == cvectc_lookup(&static_vect, ids[i]));
		}
		cvectc_stats();
	}
}

struct node {
	struct cvcentry *e;
	int              size, id, parent, lvl;
	struct node *    next, *prev;
} * start, *end;

void
print(struct cvectc *s)
{
	struct node *n   = malloc(sizeof(struct node));
	int          cnt = 0;
	assert(n);

	n->e      = &s->d;
	n->next   = NULL;
	n->prev   = NULL;
	n->size   = __cvectc_size(__cvc_dir(&s->d));
	n->parent = -1;
	n->id     = cnt++;
	n->lvl    = 0;
	start     = n;
	end       = n;

	while (start) {
		int              i;
		struct cvcentry *e;

		n     = start;
		start = n->next;
		if (n == end) end= NULL;
		if (n == start) start = NULL;
		e = n->e;

		if (__cvc_isleaf(e)) {
			struct cvcleaf *l = __cvc_leaf(e);
			printf("[l:%3d @ %p, p:%3d, n:%3d](k:%d->v:%d)\n", n->lvl, n->e, n->parent, n->id, l->id,
			       (unsigned int)l->val);
		} else {
			struct cvcdir *d = __cvc_dir(e);

			printf("[l:%3d @ %p, p:%3d, n:%3d](ignore %d)\n", n->lvl, n->e, n->parent, n->id, d->ignore);
			for (i = 0; i < n->size; i++) {
				struct node *new;
				struct cvcleaf *l;

				/* don't print empty entries */
				if (__cvc_isleaf(&d->next[i])) {
					l = __cvc_leaf(&d->next[i]);
					if (!l->id) continue;
				}
				new = malloc(sizeof(struct node));
				assert(new);
				new->e    = &d->next[i];
				new->next = NULL;
				new->prev = end;
				if (end) end->next= new;
				end = new;
				if (!start) start= new;
				/* FIXME...use real size */
				new->size   = __cvectc_size(__cvc_dir(&d->next[i]));
				new->parent = n->id;
				new->id     = cnt++;
				new->lvl    = n->lvl + 1;
			}
		}
		free(n);
	}

	return;
}

void
add(int i)
{
	assert(cvectc_add(&static_vect, (void *)i, i) >= 0);
	print(&static_vect);
	cvectc_lookup(&static_vect, i);
}

int
main(void)
{
	assert(__cvectc_prefix_match(0xF0F00000, 0xF00F0000, 8));
	assert(__cvectc_prefix_match(0xF0F00000, 0xF0F00000, 32));
	assert(__cvectc_prefix_match(0xF0F00000, 0xF0F00001, 31));
	assert(__cvectc_prefix_match(0x80F00000, 0xF0F00000, 1));
	assert(__cvectc_prefix_match(0xF0F00000, 0xF0F00008, 28));
	assert(!__cvectc_prefix_match(0xF0F00000, 0xF00F0000, 9));
	assert(!__cvectc_prefix_match(0xF0F00000, 0xF0F00001, 32));
	assert(!__cvectc_prefix_match(0x80F00000, 0xF0F00008, 2));
	assert(!__cvectc_prefix_match(0xF0F00000, 0xF0F00008, 29));

	cvectc_init(&static_vect);

	/* add(1); */
	/* add(2); */
	/* add(3); */
	/* add(7); */
	/* add(6); */
	/* add(8); */
	/* add(9); */
	/* add(100); */

	rand_test();

	return 0;
}

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

struct node {
	struct cvcentry *e;
	int size, id, parent, lvl;
	struct node *next, *prev;
} *start, *end;

void 
print(struct cvectc *s)
{
	struct node *n = malloc(sizeof(struct node));
	static int cnt = 0;
	assert(n);
	
	n->e      = &s->d;
	n->next   = NULL;
	n->prev   = NULL;
	n->size   = CVECTC_MIN_ENTRIES;
	n->parent = -1;
	n->id     = cnt++;
	n->lvl    = 0;
	start     = n;
	end       = n;
	
	while (start) {
		int i;
		struct cvcentry *e;

		printf("*\n");
		n     = start;
		start = n->next;
		if (n == end)   end   = NULL;
		if (n == start) start = NULL;
		e     = n->e;
		
		if (__cvc_isleaf(e)) {
			struct cvcleaf *l = __cvc_leaf(e);
			printf("[l:%3d, p:%3d, n:%3d](k:%d->v:%d)\n", 
			       n->lvl, n->parent, n->id, l->id, (unsigned int)l->val);
		} else {
			struct cvcdir *d = __cvc_dir(e);

			printf("[l:%3d, p:%3d, n:%3d](ignore %d)\n",
			       n->lvl, n->parent, n->id, d->ignore);
			for (i = 0 ; i < n->size ; i++) {
				struct node *new;
				struct cvcleaf *l;

				/* don't print empty entries */
				if (__cvc_leaf(&d->next[i])) {
					l = __cvc_leaf(&d->next[i]);
					if (!l->id) continue;
				}
				new = malloc(sizeof(struct node));
				assert(new);
				new->e      = &d->next[i];
				new->next   = NULL;
				new->prev   = end;
				if (end)    end->next = new;
				end         = new;
				if (!start) start = new;
				/* FIXME...use real size */
				new->size   = CVECTC_MIN_ENTRIES; 
				new->parent = n->id;
				new->id     = cnt++;
				new->lvl    = n->lvl+1;
			}
		}
		free(n);
	}

	return;
}

int main(void)
{
//	struct pair pairs[NTESTS];
//	int i;

	assert( __cvectc_prefix_match(0xF0F00000, 0xF00F0000, 8));
	assert( __cvectc_prefix_match(0xF0F00000, 0xF0F00000, 32));
	assert( __cvectc_prefix_match(0xF0F00000, 0xF0F00001, 31));
	assert( __cvectc_prefix_match(0x80F00000, 0xF0F00000, 1));
	assert( __cvectc_prefix_match(0xF0F00000, 0xF0F00008, 28));
	assert(!__cvectc_prefix_match(0xF0F00000, 0xF00F0000, 9));
	assert(!__cvectc_prefix_match(0xF0F00000, 0xF0F00001, 32));
	assert(!__cvectc_prefix_match(0x80F00000, 0xF0F00008, 2));
	assert(!__cvectc_prefix_match(0xF0F00000, 0xF0F00008, 29));

	cvectc_init(&static_vect);

	assert(cvectc_add(&static_vect, (void*)1, 1) >= 0);
	print(&static_vect);
	cvectc_lookup(&static_vect, 1);
	assert(cvectc_add(&static_vect, (void*)2, 2) >= 0);
	print(&static_vect);
	cvectc_lookup(&static_vect, 2);
	assert(cvectc_add(&static_vect, (void*)3, 3) >= 0);
	print(&static_vect);
	cvectc_lookup(&static_vect, 3);
	assert(cvectc_add(&static_vect, (void*)4, 4) >= 0);
	print(&static_vect);
	cvectc_lookup(&static_vect, 4);

	print(&static_vect);

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

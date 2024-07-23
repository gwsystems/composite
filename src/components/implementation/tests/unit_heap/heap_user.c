#include <cos_debug.h>
#include <llprint.h>
#include <assert.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <string.h>
#include <heap.h>
#include <malloc.h>
#include <posix.h>

#define VAL_BOUND 1000000

typedef enum {
	MIN = 0,
	MAX,
} heap_type_t;

int
c_min(void *a, void *b)
{
	return ((struct hentry *)a)->value <= ((struct hentry *)b)->value;
}
int
c_max(void *a, void *b)
{
	return ((struct hentry *)a)->value >= ((struct hentry *)b)->value;
}
void
u(void *e, int pos)
{
	((struct hentry *)e)->index = pos;
}

DECLARE_HEAP(min, c_min, u);
DECLARE_HEAP(max, c_max, u);

static void
entries_validate(struct heap *h, struct hentry *es, int amnt)
{
	int i;

	for (i = 0; i < amnt; i++) {
		assert(h->data[es[i].index] == &es[i]);
	}
}

static void
test_driver(int amnt, int type)
{
	int            i;
	struct hentry *prev, *es;
	struct heap *  h;

	h  = heap_alloc(amnt);
	es = malloc(sizeof(struct hentry) * amnt);
	assert(es);

	for (i = 0; i < amnt; i++) {
		es[i].value = rand() % VAL_BOUND;
		printc("adding@%d:%d\n", i, es[i].value);
		if (type == MIN) {
			assert(!min_heap_add(h, &es[i]));
			assert(!min_heap_verify(h, 1));
		} else {
			assert(!max_heap_add(h, &es[i]));
			assert(!max_heap_verify(h, 1));
		}
	}
	entries_validate(h, es, amnt);
	for (i = 0; i < amnt; i++) {
		es[i].value = rand() % VAL_BOUND;
		printc("adjusting@%d:%d\n", i, es[i].value);
		if (type == MIN) {
			min_heap_adjust(h, es[i].index);
			assert(!min_heap_verify(h, 1));
		} else {
			max_heap_adjust(h, es[i].index);    
			assert(!max_heap_verify(h, 1));
		}
	}
	entries_validate(h, es, amnt);
	prev = h->data[1];
	for (i = 0; i < amnt; i++) {
		struct hentry *curr = type == MIN ? min_heap_highest(h) : max_heap_highest(h);
		printc("highest:%d\n", curr->value);
		if (type == MIN) {    
			if (!c_min((struct hentry *)prev, (struct hentry *)curr)) assert(0);
		} else {
			if (!c_max((struct hentry *)prev, (struct hentry *)curr)) assert(0);
		}
		prev = curr;
	}
	if (type == MIN) {
		assert(!min_heap_highest(h));
		assert(!min_heap_verify(h, 1));
	} else {
		assert(!max_heap_highest(h));
		assert(!max_heap_verify(h, 1));
	}
	
	assert(heap_size(h) == 0);
	for (i = 0; i < amnt; i++) {
		es[i].value = rand() % VAL_BOUND;
		printc("adding@%d:%d\n", i, es[i].value);
		assert(!min_heap_add(h, &es[i]));
	}
	entries_validate(h, es, amnt);
	for (i = amnt; i > 0; i--) {
		int idx;
		idx                 = (rand() % i) + 1;
		struct hentry *curr = type == MIN ? min_heap_remove(h, idx) : max_heap_remove(h, idx);
		printc("removing:%d\n", curr->value);
		assert(curr);
		assert(h->e == i);
	}
	
	if (type == MIN) {
		assert(!min_heap_highest(h));
		assert(!min_heap_verify(h, 1));
	} else {
		assert(!max_heap_highest(h));
		assert(!max_heap_verify(h, 1));
	} 
	assert(heap_size(h) == 0);

	heap_destroy(h);
	free(es);
}

#define ITER 10
#define BOUND 4096

int
main(void)
{
	int i;

	srand(time(NULL));

	for (i = 0; i < ITER; i++) {
		int items = rand() % BOUND;

		printc("MIN-HEAP TEST - iter:%d items:%d\n", i, items);
		test_driver(items, MIN);
		printc("MAX-HEAP TEST - iter:%d items:%d\n", i, items);
		test_driver(items, MAX);
	}

	printc("TEST PASSED\n");
	return 0;
}

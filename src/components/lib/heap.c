#include <cos_component.h>
#include <cos_debug.h>
#include <cos_alloc.h>

#include <string.h>
#include <heap.h>

#undef HEAP_TRACE_DEBUG
#ifdef HEAP_TRACE_DEBUG
#define debug(format, ...) printc(format, ##__VA_ARGS__)
#else
#define debug(format, ...)
#endif

static inline void
swap_entries(void *arr[], int a, int b, update_fn_t u)
{
	void *t;

	t      = arr[a];
	arr[a] = arr[b];
	arr[b] = t;

	u(arr[a], a);
	u(arr[b], b);
}

/*
 * a: array
 * c: current index
 * e: end index
 */
static inline int
swap_down(struct heap *h, int c)
{
	int l; /* last entry */

	assert(c != 0);
	assert(c <= h->e);

	l = h->e - 1;
	while (c <= l / 2) { /* not a leaf? */
		int n;       /* next index */
		int left, right;

		left  = 2 * c;
		right = 2 * c + 1;

		if (right > l) {
			n = left;
		} else if (h->c(h->data[left], h->data[right])) {
			n = left;
		} else {
			n = right;
		}

		assert(n < h->e);
		if (h->c(h->data[c], h->data[n])) break; /* done? */

		swap_entries(h->data, n, c, h->u);
		c = n;
	}

	return c;
}

static inline int
swap_up(struct heap *h, int c)
{
	assert(c <= h->e);
	assert(c > 0);

	while (c > 1) {
		int p; /* parent index */

		p = c / 2;
		assert(p != 0);
		if (h->c(h->data[p], h->data[c])) break; /* done? */

		swap_entries(h->data, p, c, h->u);
		c = p;
	}
	assert(c != 0);

	return c;
}

/* return c's final index */
static inline int
heapify(struct heap *h, int c)
{
	c = swap_up(h, c);
	return swap_down(h, c);
}

#ifdef LINUX_TEST
struct hentry {
	int index, value;
};
#endif

#undef HEAP_VERIFY
#ifdef HEAP_VERIFY
static inline int
__heap_verify(struct heap *h, int c)
{
	int left, right;

	left  = c * 2;
	right = c * 2 + 1;
	if (left < h->e) {
		assert(((struct hentry *)h->data[left])->index == left);
		if (!h->c(h->data[c], h->data[left]) || __heap_verify(h, left)) {
			printc("Left data %d @ %d < %d @ %d\n", ((struct hentry *)h->data[c])->value, c,
			       ((struct hentry *)h->data[left])->value, left);
			return 1;
		}
	}
	if (right < h->e) {
		assert(((struct hentry *)h->data[right])->index == right);
		if (!h->c(h->data[c], h->data[right]) || __heap_verify(h, right)) {
			printc("Right data %d @ %d < %d @ %d\n", ((struct hentry *)h->data[c])->value, c,
			       ((struct hentry *)h->data[left])->value, left);
			return 1;
		}
	}
	return 0;
}

static int
heap_verify(struct heap *h)
{
	return __heap_verify(h, 1);
}
#else
#define heap_verify(h) 0
#endif

/* public functions */
void
heap_init(struct heap *h, int max_sz, cmp_fn_t c, update_fn_t u)
{
	assert(h);

	h->max_sz = max_sz + 1;
	h->e      = 1;
	h->c      = c;
	h->u      = u;
	h->data   = (void *)&h[1];
	assert(!heap_verify(h));
}

struct heap *
heap_alloc(int max_sz, cmp_fn_t c, update_fn_t u)
{
	struct heap *h = NULL;

#ifdef LINUX
	h = malloc(sizeof(struct heap) + (max_sz * sizeof(void *)) + 1);
	if (NULL == h) return NULL;

	heap_init(h, max_sz, c, u);
#endif

	return h;
}

void
heap_destroy(struct heap *h)
{
	assert(h && h->data);

#ifdef LINUX
	free(h);
#endif
}

int
heap_add(struct heap *h, void *new)
{
	int c;

	if (h->max_sz == h->e) return -1;

	debug("heap_add(%p,%d) %p\n", h, h->e, new);

	assert(!heap_verify(h));
	c          = h->e;
	h->data[c] = new;
	h->u(new, c);
	h->e++;
	heapify(h, c);
	assert(!heap_verify(h));

	return 0;
}

void *
heap_highest(struct heap *h)
{
	void *r;

	if (h->e == 1) return NULL;

	assert(!heap_verify(h));
	r = h->data[1];
	debug("heap_highest(%p,%d) %p\n", h, h->e, r);

	h->e--;
	h->data[1] = h->data[h->e];
	h->u(h->data[1], 1);
	swap_down(h, 1);
	assert(!heap_verify(h));
	h->u(r, 0);

	return r;
}

void *
heap_peek(struct heap *h)
{
	if (h->e == 1) return NULL;
	assert(!heap_verify(h));
	return h->data[1];
}

void
heap_adjust(struct heap *h, int c)
{
	assert(c < h->e);
	assert(c > 0);

	debug("heap_adjust(%p,%d) %p@%d\n", h, h->e, h->data[c], c);
	heapify(h, c);
	assert(!heap_verify(h));
}

void *
heap_remove(struct heap *h, int c)
{
	void *r;

	assert(c < h->e);
	assert(c >= 1);
	if (h->e == 1) return NULL;

	assert(!heap_verify(h));
	r = h->data[c];
	debug("heap_remove(%p,%d) %p@%d\n", h, h->e, h->data[c], c);
	h->e--;
	h->u(r, 0);

	if (c == h->e) {
		assert(!heap_verify(h));
		return r;
	}
	h->data[c] = h->data[h->e];
	h->u(h->data[c], c);
	heap_adjust(h, c);
	assert(!heap_verify(h));

	return r;
}

int
heap_size(struct heap *h)
{
	return h->e - 1;
}

#ifdef LINUX_TEST
#define VAL_BOUND 1000000

enum heap_type
{
	MIN = 0,
	MAX,
};

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
	cmp_fn_t       c;
	struct heap *  h;

	c  = (type == MIN ? c_min : c_max);
	h  = heap_alloc(amnt, c, u);
	es = malloc(sizeof(struct hentry) * amnt);
	assert(es);

	for (i = 0; i < amnt; i++) {
		es[i].value = rand() % VAL_BOUND;
		printf("adding@%d:%d\n", i, es[i].value);
		assert(!heap_add(h, &es[i]));
	}
	entries_validate(h, es, amnt);
	for (i = 0; i < amnt; i++) {
		es[i].value = rand() % VAL_BOUND;
		printf("adjusting@%d:%d\n", i, es[i].value);
		heap_adjust(h, es[i].index);
	}
	entries_validate(h, es, amnt);
	prev = h->data[1];
	for (i = 0; i < amnt; i++) {
		struct hentry *curr = heap_highest(h);
		printf("highest:%d\n", curr->value);
		if (!c((struct hentry *)prev, (struct hentry *)curr)) assert(0);
		prev = curr;
	}
	assert(!heap_highest(h));
	assert(heap_size(h) == 0);
	for (i = 0; i < amnt; i++) {
		es[i].value = rand() % VAL_BOUND;
		printf("adding@%d:%d\n", i, es[i].value);
		assert(!heap_add(h, &es[i]));
	}
	entries_validate(h, es, amnt);
	for (i = amnt; i > 0; i--) {
		int idx;
		idx                 = (rand() % i) + 1;
		struct hentry *curr = heap_remove(h, idx);
		printf("removing:%d\n", curr->value);
		assert(curr);
		assert(h->e == i);
	}
	assert(!heap_highest(h));
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

		printf("MIN-HEAP TEST - iter:%d items:%d\n", i, items);
		test_driver(items, MIN);
		printf("MAX-HEAP TEST - iter:%d items:%d\n", i, items);
		test_driver(items, MAX);
	}

	return 0;
}

#endif

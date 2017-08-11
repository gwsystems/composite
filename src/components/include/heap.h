#ifndef HEAP_H
#define HEAP_H

/* is a >= b? */
typedef int (*cmp_fn_t)(void *a, void *b);
/* update the integer position of an entry */
typedef void (*update_fn_t)(void *e, int pos);

struct heap {
	/* current end, and max allocated size */
	int         e, max_sz;
	cmp_fn_t    c;
	update_fn_t u;
	void **     data;
};

void  heap_adjust(struct heap *h, int c);
void *heap_remove(struct heap *h, int c);
/* return and remove from the heap the highest value */
void *heap_highest(struct heap *h);
/* return but don't remove the highest value */
void *       heap_peek(struct heap *h);
int          heap_add(struct heap *h, void *new);
void         heap_destroy(struct heap *h);
struct heap *heap_alloc(int max_sz, cmp_fn_t c, update_fn_t u);
void         heap_init(struct heap *h, int max_sz, cmp_fn_t c, update_fn_t u);
int          heap_size(struct heap *h);
static inline int
heap_empty(struct heap *h)
{
	return heap_size(h) == 0;
}

#endif /* HEAP_H */

#ifndef LIST_H
#define LIST_H

/*
 * Linked list implementation emphasizing simplicity, not memory
 * savings.  Use a pointer to the containing structure, rather than
 * pointer math based on the size of the structure.
 */

struct list_node {
	struct list_node *next, *prev;
	void *container;
};

struct list {
	struct list_node l;
};

static inline void
list_init(struct list_node *l, void *obj)
{
	l->next = l->prev = l;
	l->container = obj;
}

static inline void
list_head_init(struct list *h)
{ list_init(&h->l, NULL); }

static inline void
list_enqueue(struct list *hd, struct list_node *l)
{
	struct list_node *h = &hd->l, *p = h->prev;

	l->next = h;
	l->prev = p;
	p->next = l;
	h->prev = l;
}

static inline void
list_add(struct list *hd, struct list_node *l)
{ list_enqueue(hd, hd->l.next); }

static inline void
list_rem(struct list_node *n)
{
	n->next->prev = n->prev;
	n->prev->next = n->next;
	n->prev = n->next = n;
}

static inline int
list_empty(struct list_node *l) { return l->next == l; }

static inline void *
list_dequeue(struct list *h)
{
	struct list_node *n = h->l.next;

	if (list_empty(n)) return NULL;
	list_rem(n);

	return n->container;
}

/*
 * Iteration interface.  list_next == NULL when we iterate through the
 * entire list.
 */

static inline void *
list_next(struct list_node *n)
{ return n->next->container; } 	/* return NULL if head */

static inline void *
list_first(struct list *h) { return list_next(&h->l); }

#endif	/* LIST_H */

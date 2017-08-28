#ifndef LIST_H
#define LIST_H

/*
 * Linked list implementation emphasizing simplicity, not memory
 * savings.  Use a pointer to the containing structure, rather than
 * pointer math based on the size of the structure.
 */

struct list_node {
	struct list_node *next, *prev;
	void *            container;
};

struct list {
	struct list_node l;
};

/* struct list_node operations: */

static inline void
list_init(struct list_node *l, void *obj)
{
	l->next = l->prev = l;
	l->container      = obj;
}

static inline void
list_add_before(struct list_node *l, struct list_node *new)
{
	new->next     = l;
	new->prev     = l->prev;
	l->prev->next = new;
	l->prev       = new;
}

static inline void
list_add_after(struct list_node *l, struct list_node *new)
{
	new->next     = l->next;
	new->prev     = l;
	l->next->prev = new;
	l->next       = new;
}

static inline void
list_rem(struct list_node *n)
{
	n->next->prev = n->prev;
	n->prev->next = n->next;
	n->prev = n->next = n;
}

static inline int
list_empty(struct list_node *l)
{
	return l->next == l;
}

/* struct list operations: */

static inline void
list_head_init(struct list *h)
{
	list_init(&h->l, NULL);
}

/* add to end */
static inline void
list_enqueue(struct list *hd, struct list_node *l)
{
	list_add_after(hd->l.prev, l);
}

/* add to start */
static inline void
list_add(struct list *hd, struct list_node *l)
{
	list_add_after(&hd->l, l);
}

/* dequeue from start */
static inline void *
list_dequeue(struct list *h)
{
	struct list_node *n = h->l.next;

	list_rem(n);

	return n->container;
}

/*
 * Iteration interface.
 *
 * for (o = list_first(&list) ; o ; o = list_next(&o->list)) {...}
 */

static inline void *
list_next(struct list_node *n)
{
	return n->next->container;
} /* return NULL if head */

static inline void *
list_first(struct list *h)
{
	return list_next(&h->l);
}

/* is list empty */
static inline int
list_isempty(struct list *h)
{
	return list_first(h) == NULL;
}

#endif /* LIST_H */

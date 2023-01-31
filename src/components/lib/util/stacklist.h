#ifndef STACKLIST_H
#define STACKLIST_H

/**
 * Modified to support multi-core via a Treiber stack. This is not 100%
 * a great solution as it isn't FIFO. However, we release *all*
 * threads when unlocking, so the priority scheduling should take over
 * at that point.
 */

#include <cos_component.h>
#include <ps.h>

struct stacklist {
	struct stacklist *next;
	void *data;
};

struct stacklist_head {
	struct stacklist *head;
};

static inline void
stacklist_init(struct stacklist_head *h)
{
	h->head = NULL;
}

/*
 * Remove a thread from the list that has been woken. Return 0 on
 * success, and 1 if it could not be removed.
 */
static inline int
stacklist_rem(struct stacklist *l)
{
	/*
	 * Not currently supported with Trebor Stack. Threads that
	 * wake early still have to wait their turn.
	 */
	return 1;
}

/* Add a thread that is going to block */
static inline void
stacklist_add(struct stacklist_head *h, struct stacklist *l, void *data)
{
	l->next = NULL;
	l->data = data;
	assert(h);

	while (1) {
		struct stacklist *n = ps_load(&h->head);

		l->next = n;
		if (ps_cas((unsigned long *)&h->head, (unsigned long)n, (unsigned long)l)) break;
	}
}

/* Get a thread to wake up, and remove its record! */
static inline struct stacklist *
stacklist_dequeue(struct stacklist_head *h)
{
	struct stacklist *sl;

	if (!h->head) return NULL;

	/*
	 * Only a single thread should trigger an event, and dequeue
	 * threads, but we'll implement this conservatively. Given
	 * this, please note that this should *not* iterate more than
	 * once.
	 */
	while (1) {
		sl = ps_load(&h->head);

		if (ps_cas((unsigned long *)&h->head, (unsigned long)sl, (unsigned long)sl->next)) break;
	}
	sl->next = NULL;

	return sl;
}

/*
 * A thread that wakes up after blocking using a stacklist should be
 * able to assume that it is no longer on the list. This enables them
 * to assert on that fact.
 */
static inline int
stacklist_is_removed(struct stacklist *l)
{
	return l->next == NULL;
}

#endif	/* STACKLIST_H */

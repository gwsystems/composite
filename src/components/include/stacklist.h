#ifndef STACKLIST_H
#define STACKLIST_H

#include <cos_component.h>
#include <ps_list.h>

struct stacklist_head {
	struct ps_list_head head;
};

struct stacklist {
	thdid_t thdid;
	struct ps_list list;
};

static inline void
stacklist_init(struct stacklist_head *h)
{
	ps_list_head_init(&h->head);
}

/* Remove a thread from the list that has been woken */
static inline void
stacklist_rem(struct stacklist *l)
{
	ps_list_rem_d(l);
}

/* Add a thread that is going to block */
static inline void
stacklist_add(struct stacklist_head *h, struct stacklist *l)
{
	ps_list_init_d(l);
	ps_list_head_add_d(&h->head, l);
	l->thdid = cos_thdid();
}

/* Get a thread to wake up, and remove its record! */
static inline thdid_t
stacklist_dequeue(struct stacklist_head *h)
{
	struct stacklist *sl;

	if (ps_list_head_empty(&h->head)) return 0;

	sl = ps_list_head_first_d(&h->head, struct stacklist);
	stacklist_rem(sl);

	return sl->thdid;
}

#endif	/* STACKLIST_H */

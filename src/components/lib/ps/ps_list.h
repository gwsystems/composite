/***
 * Copyright 2009-2015 by Gabriel Parmer.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gparmer@gwu.edu, 2015
 *
 * History:
 * - Initial implementation, ~2009
 * - Adapted for parsec, 2015
 */

#ifndef PS_LIST_H
#define PS_LIST_H

struct ps_list {
	void *next, *prev;
};

#define PS_LIST_STATIC_INIT(obj, l)		  \
	obj = {                                   \
		.l.next = &obj,			  \
		.l.prev = &obj			  \
	}

#define ps_list_init(obj, l)					\
	do { (obj)->l.next = (obj)->l.prev = (obj); } while (0)

#define ps_list_first(obj, l) ((typeof(obj))((obj)->l.next))
#define ps_list_last(obj, l)  ((typeof(obj))((obj)->l.prev))
#define ps_list_next(obj, l)  ps_list_first(obj, l)
#define ps_list_prev(obj, l)  ps_list_last(obj, l)

/*
 * FIXME: add compiler barrier after setting the new node's ->next
 * value.
 *
 * TODO: Provide a variant on this macro that uses cas for the
 * modification.
 */
#define ps_list_add(head, new, l) do {		  \
	(new)->l.next = (head)->l.next;		  \
	(new)->l.prev = (head);			  \
	(head)->l.next = (new);			  \
	ps_list_next(new, l)->l.prev = (new); } while (0)

/* Note we don't reset ->next as there might be concurrent reads */
#define ps_list_rem(obj, l) do {				\
	ps_list_next(obj, l)->l.prev = (obj)->l.prev;           \
	ps_list_prev(obj, l)->l.next = (obj)->l.next;           \
	(obj)->l.prev = (obj)->l.next = (obj); } while (0)

#define ps_list_empty(obj, l)			                \
	((obj)->l.prev == (obj))

#define ps_list_append(head, new, l)			        \
	ps_list_add(ps_list_last(head, l), new, l)

#endif	/* PS_LIST_H */

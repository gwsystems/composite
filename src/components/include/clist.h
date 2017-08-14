/*
 * GPL v2, copyright Gabriel Parmer, 2013
 */

#ifndef CLIST_H
#define CLIST_H

#ifdef LINUX_TEST
#include <assert.h>
#else
#include <cos_debug.h>
#endif
#include <stddef.h> /* offsetof */

struct clist {
	struct clist *n, *p;
};

/*
 * This is a separate type to 1) provide guidance on how to use the
 * API, and 2) to prevent developers from comparing pointers that
 * should not be compared.
 */
struct clist_head {
	struct clist l;
};

#define CLIST_DEF_NAME list

static inline void
clist_ll_init(struct clist *l)
{
	l->n = l->p = l;
}

static inline void
clist_head_init(struct clist_head *lh)
{
	clist_ll_init(&lh->l);
}

static inline int
clist_ll_empty(struct clist *l)
{
	return l->n == l;
}

static inline int
clist_head_empty(struct clist_head *lh)
{
	return clist_ll_empty(&lh->l);
}

static inline void
clist_ll_add(struct clist *l, struct clist *new)
{
	new->n    = l->n;
	new->p    = l;
	l->n      = new;
	new->n->p = new;
}

static inline void
clist_ll_rem(struct clist *l)
{
	l->n->p = l->p;
	l->p->n = l->n;
	l->p = l->n = l;
}

/*
 * Get a pointer to the object containing *l, of a type shared with
 * *o.  Importantly, "o" is not accessed here, and is _only_ used for
 * its type.  It will typically be the iterator/cursor working through
 * a list.  Do _not_ use this function.  It is a utility used by the
 * following functions.
 */
#define clist_obj_get_l(l, o, lname) (typeof(*(o)) *)(((char *)(l)) - offsetof(typeof(*(o)), lname))

/***
 * The object API.  These functions are called with pointers to your
 * own (typed) structures.
 */

/*
 * If iterating through the list, this will tell you if the object you
 * retrieve is the head.  For example, you must:
 *
 * for (clist_head_fst(lh, &o) ; !clist_is_head(lh, o) ; o = clist_next(o)) {...}
 *
 * as this will not work (as o will point to the phantom object above o):
 *
 * for (clist_head_fst(lh, &o) ; lh != o ; o = clist_next(o)) {...}
 *
 * In fact, you can never access clist_next(o) without first checking
 * that it is not the head as that pointer is invalid.
 */
#define clist_is_head_l(lh, o, lname) (clist_obj_get_l((lh), (o), lname) == (o))

/* functions for if we don't use the default name for the list field */
#define clist_singleton_l(o, lname) clist_ll_empty(&(o)->lname)
#define clist_init_l(o, lname) clist_ll_init(&(o)->lname)
#define clist_next_l(o, lname) clist_obj_get_l((o)->lname.n, (o), lname)
#define clist_prev_l(o, lname) clist_obj_get_l((o)->lname.p, (o), lname)
#define clist_add_l(o, n, lname) clist_ll_add(&(o)->lname, &(n)->lname)
#define clist_append_l(o, n, lname) clist_add_l(clist_prev_l((o), lname), n, lname)
#define clist_rem_l(o, lname) clist_ll_rem(&(o)->lname)
#define clist_head_add_l(lh, o, lname) clist_ll_add((&(lh)->l), &(o)->lname)
#define clist_head_append_l(lh, o, lname) clist_ll_add(((&(lh)->l)->p), &(o)->lname)

/*
 * For these functions, "o" must be a pointer to a pointer to a struct
 * of the type that populates the linked list.  The pointer will get
 * set to the first (last) object in the list.  "o" is typically a
 * pointer to the iterator that is stepping through the list.  "lh" is
 * the list head.
 */
#define clist_head_fst_l(lh, o, lname) (*(o) = clist_obj_get_l(((lh)->l.n), (*o), lname))
#define clist_head_lst_l(lh, o, lname) (*(o) = clist_obj_get_l(((lh)->l.p), (*o), lname))

/* If your struct named the list field "list" (as defined by CLIST_DEF_NAME */
#define clist_is_head(lh, o) clist_is_head_l(lh, o, CLIST_DEF_NAME)
#define clist_singleton(o) clist_singleton_l(o, CLIST_DEF_NAME)
#define clist_init(o) clist_init_l(o, CLIST_DEF_NAME)
#define clist_next(o) clist_next_l(o, CLIST_DEF_NAME)
#define clist_prev(o) clist_prev_l(o, CLIST_DEF_NAME)
#define clist_add(o, n) clist_add_l(o, n, CLIST_DEF_NAME)
#define clist_append(o, n) clist_append_l(o, n, CLIST_DEF_NAME)
#define clist_rem(o) clist_rem_l(o, CLIST_DEF_NAME)
#define clist_head_fst(lh, o) clist_head_fst_l(lh, o, CLIST_DEF_NAME)
#define clist_head_lst(lh, o) clist_head_lst_l(lh, o, CLIST_DEF_NAME)
#define clist_head_add(lh, o) clist_head_add_l(lh, o, CLIST_DEF_NAME)
#define clist_head_append(lh, o) clist_head_append_l(lh, o, CLIST_DEF_NAME)

#endif /* CLIST_H */

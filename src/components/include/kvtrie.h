/**
 * Copyright 2014 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 * 
 * This is a simple key-value radix-tree.  Simple: it maps between ids
 * to pointers.  Notably, it does _not_ use a hash table.  This is
 * good, or bad, depending on your goals and context.  The size of
 * each internal node is configurable, and directly impacts the depth
 * of the tree.  Specifically, if you have an required id space of N
 * bits, and you want the internal nodes to have I entries, and the
 * leaf nodes to have L entries, then the depth, D = ceil((N-L)/I)+1.
 */

#ifndef KVTRIE_H
#define KVTRIE_H

#include <ertrie.h>

/***
 * This file provides a key-value radix trie that does a conversion
 * between an id (key) and a pointer value.  These functions are
 * intended to mainly be a replacement for those provided in ertrie.h
 * as they are more specialized and easier to use for lookups that
 * conform to this format.  The only functions from ertrie.h that are
 * still just as useful are dsname_alloc and dsname_maxid.
 */

#define KVT_CREATE(name, depth, order, last_order, initval, initfn, getfn, isnullfn, setfn, allocfn, freefn) \
ERT_CREATE(name, depth, order, last_order, sizeof(int*), initval, initfn, getfn, isnullfn, setfn, allocfn, freefn) \
static void name##_free(struct ert *v)					\
{ kvt_free(v, depth, order, last_order, sizeof(int*), initval, initfn, getfn, isnullfn, setfn, allocfn, freefn); } \
static inline void *name##_lkupp(struct ert *v, unsigned long id)	\
{ return kvt_lkupp(v, id, depth, order, last_order, sizeof(int*), initval, initfn, getfn, isnullfn, setfn, allocfn, freefn); } \
static inline int name##_add(struct ert *v, long id, void *val)		\
{ return kvt_add(v, id, val, depth, order, last_order, sizeof(int*), initval, initfn, getfn, isnullfn, setfn, allocfn, freefn); } \
static inline int name##_del(struct ert *v, long id)			\
{ return kvt_del(v, id, depth, order, last_order, sizeof(int*), initval, initfn, getfn, isnullfn, setfn, allocfn, freefn); }

#define KVT_CREATE_DEF(name, depth, order, last_order, allocfn, freefn)	\
KVT_CREATE(name, depth, order, last_order, ert_definitval, ert_definitfn, ert_defget, ert_defisnull, ert_defset, allocfn, freefn)

/* 
 * Lookup a value that is pointed to by the trie (p = ptr).  Only use
 * this function if the last_sz == sizeof a pointer.  This is enforced
 * via an assertion.  It doesn't make sense to dereference the pointer
 * in the last level of the tree if it isn't a pointer.
 */
static inline void *
kvt_lkupp(struct ert *v, unsigned long id, ERT_CONST_PARAMS)
{
	struct ert_intern *vi;
	unsigned long accum;

	assert(last_sz == sizeof(int*));
	vi = __ert_lookup(v, id, depth, &accum, ERT_CONST_ARGS);
	if (unlikely(!vi)) return NULL;
	return getfn(vi->next, &accum, 1);
}

static inline int 
__kvt_set(struct ert *v, long id, void *val, ERT_CONST_PARAMS)
{
	struct ert_intern *vi;
	unsigned long accum = 0;
	
	vi = __ert_lookup(v, id, depth, &accum, ERT_CONST_ARGS);
	if (unlikely(!vi)) return -1;
	setfn(vi, val, 1);

	return 0;
}

/* 
 * This function will try to find an empty slot specifically for the
 * identifier id, or fail.  This is essentially a expand + set +
 * lookup.
 *
 * This _cannot_ be used with ertries that have a last level node size
 * greater than a pointer.
 *
 * Will return NULL if 1) id already exists, 2) could not expand the
 * structure (e.g. memory allocation failure), or 3) the value could
 * not be set in the structure.
 */
static inline int
kvt_add(struct ert *v, unsigned long id, void *val, ERT_CONST_PARAMS)
{
	void *p;

	assert(v);
	assert(val != initval);
	assert(id < ERT_MAX_ID);
	assert(last_sz == sizeof(int*));
	if (unlikely(kvt_lkupp(v, id, ERT_CONST_ARGS))) return 1;
	if (__kvt_set(v, id, val, ERT_CONST_ARGS)) {
		if (__ert_expand(v, id, depth, NULL, ERT_CONST_ARGS)) return 1;
		if (__kvt_set(v, id, val, ERT_CONST_ARGS))            return 1;
	}
	p = kvt_lkupp(v, id, ERT_CONST_ARGS);
	assert(p == val);

	return 0;
}

/* 
 * Set the node corresponding to id to the initial value.  If we
 * cannot reset the value (e.g. because the tree is incomplete and not
 * completely allocated, or because we're trying to set a node that is
 * not already used in the trie), we return 1.
 */
static inline int 
kvt_del(struct ert *v, unsigned long id, ERT_CONST_PARAMS)
{
	assert(v);
	assert(id < ERT_MAX_ID);
	assert(last_sz == sizeof(int*));
	if (__kvt_set(v, id, initval, ERT_CONST_ARGS)) return 1;
	assert(!kvt_lkupp(v, id, ERT_CONST_ARGS));

	return 0;
}

static inline void 
__kvt_free_rec(struct ert_intern *vi, u32_t lvl, ERT_CONST_PARAMS)
{
	int i, sz;
	unsigned long accum = 0;
	
	assert(vi);
	if (lvl > 1) {
		for (i = 0 ; i < (1<<order) ; i++) {
			if (isnullfn(vi[i].next, &accum, 0)) continue;
			__kvt_free_rec(vi[i].next, lvl-1, ERT_CONST_ARGS);
		}
	}
	if (lvl > 1) sz = 1<<order * sizeof(int*);
	else         sz = 1<<last_order * last_sz;
	freefn(NULL, vi, sz, lvl <= 1); 
}

/* 
 * Beware of using this.  It is recursive, is unbounded in latency for
 * all practical purposes.  This should also only really be called if
 * there are not values stored in the tree.
 */
static void 
kvt_free(struct ert *v, ERT_CONST_PARAMS)
{
	assert(v);
	__kvt_free_rec(v->vect, depth, ERT_CONST_ARGS);
}


#endif /* KVTRIE_H */

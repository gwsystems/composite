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
typedef void (*kv_free_fn_t)(void *data, void *mem, int sz, int leaf);

#define __KVT_CREATE(name, depth, order, last_order, initval, initfn, getfn, isnullfn, setfn, allocfn, freefn,         \
                     setleaffn, getleaffn, resolvefn)                                                                  \
	ERT_CREATE(name, name##_ert, depth, order, sizeof(int *), last_order, sizeof(int *), initval, initfn, getfn,   \
	           isnullfn, setfn, allocfn, setleaffn, getleaffn, resolvefn)                                          \
	static void name##_free(struct name##_ert *v)                                                                  \
	{                                                                                                              \
		__kvt_free((struct ert *)v, depth, order, sizeof(int *), last_order, sizeof(int *), initval, initfn,   \
		           getfn, isnullfn, setfn, allocfn, setleaffn, getleaffn, resolvefn, freefn);                  \
	}                                                                                                              \
	static inline void *name##_lkupp(struct name##_ert *v, unsigned long id)                                       \
	{                                                                                                              \
		unsigned long accum;                                                                                   \
		return __ert_lookup((struct ert *)v, id, 0, depth + 1, &accum, depth, order, sizeof(int *),            \
		                    last_order, sizeof(int *), initval, initfn, getfn, isnullfn, setfn, allocfn,       \
		                    setleaffn, getleaffn, resolvefn);                                                  \
	}                                                                                                              \
	static inline int name##_add(struct name##_ert *v, long id, void *val)                                         \
	{                                                                                                              \
		return __kvt_add((struct ert *)v, id, val, depth, order, sizeof(int *), last_order, sizeof(int *),     \
		                 initval, initfn, getfn, isnullfn, setfn, allocfn, setleaffn, getleaffn, resolvefn);   \
	}                                                                                                              \
	static inline int name##_del(struct name##_ert *v, long id)                                                    \
	{                                                                                                              \
		return __kvt_del((struct ert *)v, id, depth, order, sizeof(int *), last_order, sizeof(int *), initval, \
		                 initfn, getfn, isnullfn, setfn, allocfn, setleaffn, getleaffn, resolvefn);            \
	}

#define KVT_CREATE(name, depth, order, last_order, allocfn, freefn)                                            \
	__KVT_CREATE(name, depth, order, last_order, NULL, ert_definit, ert_defget, ert_defisnull, ert_defset, \
	             allocfn, freefn, ert_defsetleaf, ert_defgetleaf, ert_defresolve)

#define KVT_ROUND(a, sz) (((unsigned long)a) & (~(sz - 1)))
static int
ert_nonull_isnull(struct ert_intern *a, void *accum, int isleaf)
{
	(void)isleaf;
	if (!accum) return a->next == NULL; /* optimized away in lookup path */
	return 0;                           /* no conditional! */
}

struct ert_intern test_sink[1 << 9] = {
  {.next = test_sink},
};

/*
 * The allocfn and freefn MUST align the nodes on their natural size
 * boundary (i.e. page nodes must be aligned on a page).
 */
#define KVT_CREATE_ALIGNED(name, depth, order, last_order, allocfn, freefn)                                         \
	struct ert_intern name##_sink[1 << order] = {                                                               \
	  {.next = name##_sink},                                                                                    \
	};                                                                                                          \
	static void name##_nonull_initfn(struct ert_intern *a, int leaf)                                            \
	{                                                                                                           \
		if (leaf)                                                                                           \
			a->next = NULL;                                                                             \
		else                                                                                                \
			a->next = name##_sink;                                                                      \
	}                                                                                                           \
	static struct int name##_nonull_resolve(struct ert_intern *a, void *accum, int leaf, u32_t order, u32_t sz) \
	{                                                                                                           \
		(void)leaf;                                                                                         \
		if (!accum) return 1;                                                                               \
		return KVT_ROUND(*a, ((1 << order) * sz)) != KVT_ROUND(name##_sink, ((1 << order) * sz))            \
	}                                                                                                           \
	__KVT_CREATE(name, depth, order, last_order, NULL, name##_nonnull_initfn, ert_defget, ert_defisnull,        \
	             ert_defset, allocfn, freefn, ert_defsetleaf, ert_defgetleaf, name##_nonnull_resolve)

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
__kvt_add(struct ert *v, unsigned long id, void *val, ERT_CONST_PARAMS)
{
	int ret;

	assert(v);
	assert(val != initval);
	assert(id < __ert_maxid(ERT_CONST_ARGS));
	assert(last_sz == sizeof(int *));
	if (unlikely(__ert_lookup(v, id, 0, depth + 1, NULL, ERT_CONST_ARGS))) return 1;
	ret = __ert_expand(v, id, 0, depth + 1, NULL, NULL, val, ERT_CONST_ARGS);
	assert(val == __ert_lookup(v, id, 0, depth + 1, NULL, ERT_CONST_ARGS));

	return ret;
}

/*
 * Set the node corresponding to id to the initial value.  If we
 * cannot reset the value (e.g. because the tree is incomplete and not
 * completely allocated, or because we're trying to set a node that is
 * not already used in the trie), we return 1.
 */
static inline int
__kvt_del(struct ert *v, unsigned long id, ERT_CONST_PARAMS)
{
	int ret;

	assert(v);
	assert(id < __ert_maxid(ERT_CONST_ARGS));
	assert(last_sz == sizeof(int *));
	ret = __ert_expand(v, id, 0, depth + 1, NULL, NULL, initval, ERT_CONST_ARGS);
	assert(!__ert_lookup(v, id, 0, depth + 1, NULL, ERT_CONST_ARGS));

	return 0;
}

static inline void
__kvt_free_rec(struct ert_intern *vi, u32_t lvl, ERT_CONST_PARAMS, kv_free_fn_t freefn)
{
	int i, sz;

	assert(vi);
	if (lvl > 1) {
		for (i = 0; i < (1 << order); i++) {
			if (isnullfn(&vi[i], NULL, 0)) continue;
			__kvt_free_rec(getfn(&vi[i], NULL, 0), lvl - 1, ERT_CONST_ARGS, freefn);
		}
	}
	if (lvl > 1)
		sz = (1 << order) * intern_sz;
	else
		sz = (1 << last_order) * last_sz;
	freefn(NULL, vi, sz, lvl <= 1);
}

/*
 * Beware of using this.  It is recursive, is unbounded in latency for
 * all practical purposes.  This should also only really be called if
 * there are not values stored in the tree.
 */
static void
__kvt_free(struct ert *v, ERT_CONST_PARAMS, kv_free_fn_t freefn)
{
	assert(v);
	__kvt_free_rec(v->vect, depth, ERT_CONST_ARGS, freefn);
}


#endif /* KVTRIE_H */

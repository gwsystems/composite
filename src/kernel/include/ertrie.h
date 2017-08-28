/**
 * Copyright 2014 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef ERTRIE_H
#define ERTRIE_H

#include "cc.h"

#ifndef TYPES_H
#include "shared/cos_types.h"
#endif

#ifndef COS_COMPONENT_H
/* for kernel level use only */
#include "chal.h"
#include <assert.h>
#endif

#define CFORCEINLINE __attribute__((always_inline))

/*
 * TODO:
 * - change the accum variable to be void *, and be named load_info,
 *   and memctxt to be named store_info.
 */

/* Internal node in the trie */
struct ert_intern {
	/*
	 * This "next" value should be opaque and only interpreted by
	 * the specialized functions.  It might be a pointer, or
	 * array.
	 */
	void *next;
};
struct ert {
	struct ert_intern vect[0]; /* in-place data-structure */
};

/* get the next level/value in the internal structure/value */
typedef struct ert_intern *(*ert_get_fn_t)(struct ert_intern *, void *accum, int isleaf);
/* check if the value in the internal structure is "null" */
typedef int (*ert_isnull_fn_t)(struct ert_intern *, void *accum, int isleaf);
/* does this final ert_intern in a lookup resolve to an successful lookup? */
typedef int (*ert_resolve_fn_t)(struct ert_intern *a, void *accum, int leaf, u32_t order, u32_t sz);
/* set values to their initial value (often "null") */
typedef void (*ert_initval_fn_t)(struct ert_intern *, int isleaf);
/* set a value in an internal structure/value */
typedef int (*ert_set_fn_t)(struct ert_intern *e, void *val, void *accum, int isleaf);
/* allocate an internal or leaf structure */
typedef void *(*ert_alloc_fn_t)(void *data, int sz, int last_lvl);
/* if we you extending the leaf level, this is called to set the leaf entry */
typedef int (*ert_setleaf_fn_t)(struct ert_intern *entry, void *data);
typedef void *(*ert_getleaf_fn_t)(struct ert_intern *entry, void *accum);

#define ERT_CONST_PARAMS                                                                             \
	u32_t depth, u32_t order, u32_t intern_sz, u32_t last_order, u32_t last_sz, void *initval,   \
	  ert_initval_fn_t initfn, ert_get_fn_t getfn, ert_isnull_fn_t isnullfn, ert_set_fn_t setfn, \
	  ert_alloc_fn_t allocfn, ert_setleaf_fn_t setleaffn, ert_getleaf_fn_t getleaffn, ert_resolve_fn_t resolvefn
#define ERT_CONST_ARGS                                                                                             \
	depth, order, intern_sz, last_order, last_sz, initval, initfn, getfn, isnullfn, setfn, allocfn, setleaffn, \
	  getleaffn, resolvefn
#define ERT_CONSTS_DEWARN \
	(void)depth;      \
	(void)order;      \
	(void)intern_sz;  \
	(void)last_order; \
	(void)last_sz;    \
	(void)initval;    \
	(void)initfn;     \
	(void)getfn;      \
	(void)isnullfn;   \
	(void)setfn;      \
	(void)allocfn;    \
	(void)setleaffn;  \
	(void)getleaffn;  \
	(void)resolvefn;
#define ERT_DEWARN ERT_CONSTS_DEWARN

/*
 * Default implementations of the customization functions that assume
 * a normal tree with pointers for internal nodes, with the "null
 * node" being equal to NULL (i.e. you can't store NULL values in the
 * structure), and setting values in internal and leaf nodes being
 * done with straightforward stores.
 */
static inline CFORCEINLINE struct ert_intern *
ert_defget(struct ert_intern *a, void *accum, int leaf)
{
	(void)accum;
	(void)leaf;
	return a->next;
}
static inline void *
ert_defgetleaf(struct ert_intern *a, void *accum)
{
	(void)accum;
	return a->next;
}
static inline int
ert_defisnull(struct ert_intern *a, void *accum, int leaf)
{
	(void)accum;
	(void)leaf;
	return a->next == NULL;
}
static int
ert_defresolve(struct ert_intern *a, void *accum, int leaf, u32_t order, u32_t sz)
{
	(void)a;
	(void)accum;
	(void)leaf;
	(void)order;
	(void)sz;
	return 1;
}
static int
ert_defset(struct ert_intern *a, void *v, void *accum, int leaf)
{
	(void)leaf;
	(void)accum;
	a->next = v;
	return 0;
}
static int
ert_defsetleaf(struct ert_intern *a, void *data)
{
	a->next = data;
	return 0;
}
static void
ert_definit(struct ert_intern *a, int leaf)
{
	(void)a;
	(void)leaf;
	a->next = NULL;
}

/*
 * This macro is the key using the compiler to generate fast code.
 * This is generating function calls that are often inlined that are
 * being passed _constants_.  After function inlining, loop unrolling,
 * and constant propagation, the code generated should be very
 * specific to the set of parameters used.  Loops should be
 * eliminated, conditionals removed, and straight-line code produced.
 *
 * The informal goal of this is to ensure that the lookup code
 * generated is on the order of 10-20 instructions, depending on
 * depth.  In terms of (hot-cache) performance, we're shooting for
 * ~5*depth cycles (if L1 is 5 cycles to access).  For cold caches,
 * we're looking for ~500*depth cycles (if memory accesses are 500
 * cycles).  When there is parallel contention (writes), the cost
 * should be comparable to the latter case.
 *
 * Question: How can I replace the long argument lists here with a
 * macro?  I'm hitting some preprocessor limitation I didn't
 * anticipate here.
 */
#define ERT_CREATE(name, structname, depth, order, intern_sz, last_order, last_sz, initval, initfn, getfn, isnullfn,  \
                   setfn, allocfn, setleaffn, getleaffn, resolvefn)                                                   \
	struct structname {                                                                                           \
		struct ert t;                                                                                         \
	};                                                                                                            \
	static struct structname *name##_alloc(void *memctxt)                                                         \
	{                                                                                                             \
		return (struct structname *)ert_alloc(memctxt, depth, order, intern_sz, last_order, last_sz, initval, \
		                                      initfn, getfn, isnullfn, setfn, allocfn, setleaffn, getleaffn,  \
		                                      resolvefn);                                                     \
	}                                                                                                             \
	static inline void *name##_lkup(struct structname *v, unsigned long id)                                       \
	{                                                                                                             \
		unsigned long a;                                                                                      \
		return __ert_lookup((struct ert *)v, id, 0, depth, &a, depth, order, intern_sz, last_order, last_sz,  \
		                    initval, initfn, getfn, isnullfn, setfn, allocfn, setleaffn, getleaffn,           \
		                    resolvefn);                                                                       \
	}                                                                                                             \
	static inline void *name##_lkupa(struct structname *v, unsigned long id, void *accum)                         \
	{                                                                                                             \
		return __ert_lookup((struct ert *)v, id, 0, depth, accum, depth, order, intern_sz, last_order,        \
		                    last_sz, initval, initfn, getfn, isnullfn, setfn, allocfn, setleaffn, getleaffn,  \
		                    resolvefn);                                                                       \
	}                                                                                                             \
	static inline void *name##_lkupan(struct structname *v, unsigned long id, u32_t dlimit, void *accum)          \
	{                                                                                                             \
		return __ert_lookup((struct ert *)v, id, 0, dlimit, accum, depth, order, intern_sz, last_order,       \
		                    last_sz, initval, initfn, getfn, isnullfn, setfn, allocfn, setleaffn, getleaffn,  \
		                    resolvefn);                                                                       \
	}                                                                                                             \
	static inline void *name##_lkupani(struct structname *v, unsigned long id, u32_t dstart, u32_t dlimit,        \
	                                   void *accum)                                                               \
	{                                                                                                             \
		return __ert_lookup((struct ert *)v, id, dstart, dlimit, accum, depth, order, intern_sz, last_order,  \
		                    last_sz, initval, initfn, getfn, isnullfn, setfn, allocfn, setleaffn, getleaffn,  \
		                    resolvefn);                                                                       \
	}                                                                                                             \
	static inline int name##_expandni(struct structname *v, unsigned long id, u32_t dstart, u32_t dlimit,         \
	                                  void *accum, void *memctxt, void *data)                                     \
	{                                                                                                             \
		return __ert_expand((struct ert *)v, id, dstart, dlimit, accum, memctxt, data, depth, order,          \
		                    intern_sz, last_order, last_sz, initval, initfn, getfn, isnullfn, setfn, allocfn, \
		                    setleaffn, getleaffn, resolvefn);                                                 \
	}                                                                                                             \
	static inline int name##_expandn(struct structname *v, unsigned long id, u32_t dlimit, void *accum,           \
	                                 void *memctxt, void *data)                                                   \
	{                                                                                                             \
		return __ert_expand((struct ert *)v, id, 0, dlimit, accum, memctxt, data, depth, order, intern_sz,    \
		                    last_order, last_sz, initval, initfn, getfn, isnullfn, setfn, allocfn, setleaffn, \
		                    getleaffn, resolvefn);                                                            \
	}                                                                                                             \
	static inline int name##_expand(struct structname *v, unsigned long id, void *accum, void *memctxt,           \
	                                void *data)                                                                   \
	{                                                                                                             \
		return __ert_expand((struct ert *)v, id, 0, depth, accum, memctxt, data, depth, order, intern_sz,     \
		                    last_order, last_sz, initval, initfn, getfn, isnullfn, setfn, allocfn, setleaffn, \
		                    getleaffn, resolvefn);                                                            \
	}                                                                                                             \
	static inline unsigned long name##_maxid(void)                                                                \
	{                                                                                                             \
		return __ert_maxid(depth, order, intern_sz, last_order, last_sz, initval, initfn, getfn, isnullfn,    \
		                   setfn, allocfn, setleaffn, getleaffn, resolvefn);                                  \
	}                                                                                                             \
	static inline u32_t name##_maxdepth(void) { return (u32_t)depth; }


#define ERT_CREATE_DEF(name, depth, order, last_order, last_sz, allocfn)                                              \
	ERT_CREATE(name, name##_ert, depth, order, sizeof(int *), last_order, last_sz, NULL, ert_definit, ert_defget, \
	           ert_defisnull, ert_defset, allocfn, ert_defsetleaf, ert_defgetleaf, ert_defresolve)

static inline unsigned long __ert_maxid(ERT_CONST_PARAMS)
{
	unsigned long off    = (unsigned long)(((order * (depth - 1)) + last_order));
	unsigned long maxoff = (unsigned long)(sizeof(int *) * 8); /* 8 bits per byte */
	ERT_CONSTS_DEWARN;

	return (off > maxoff) ? ((unsigned long)1) << maxoff : ((unsigned long)1) << off;
}

/*
 * Initialize a level in the ertrie.  lvl is either an internal level,
 * lvl > 1, or a leaf level in the tree in which case embedded leaf
 * structs require a different initialization.
 */
static inline void
__ert_init(struct ert_intern *vi, int isleaf, ERT_CONST_PARAMS)
{
	int i, base, sz;
	ERT_CONSTS_DEWARN;

	assert(vi);
	if (!isleaf) {
		base = 1 << order;
		sz   = intern_sz;
	} else {
		base = 1 << last_order;
		sz   = last_sz;
	}
	for (i = 0; i < base; i++) {
		struct ert_intern *t = (void *)(((char *)vi) + (sz * i));
		initfn(t, isleaf);
	}
}

static struct ert *
ert_alloc(void *memctxt, ERT_CONST_PARAMS)
{
	struct ert *      v;
	struct ert_intern e;
	unsigned long     accum = 0;

	/* Make sure the id size can be represented on our system */
	assert(((order * (depth - 1)) + last_order) < (sizeof(unsigned long) * 8));
	assert(depth >= 1);
	if (depth > 1)
		v = allocfn(memctxt, (1 << order) * intern_sz, 0);
	else
		v = allocfn(memctxt, (1 << last_order) * last_sz, 1);
	if (NULL == v) return NULL;
	__ert_init(v->vect, depth == 1, ERT_CONST_ARGS);

	setfn(&e, v, &accum, depth == 1);
	return (struct ert *)e.next;
}

static inline struct ert_intern *
__ert_walk(struct ert_intern *vi, unsigned long id, void *accum, u32_t lvl, ERT_CONST_PARAMS)
{
	u32_t last_off;
#define ERT_M(id, o) ((id) & ((1 << (o)) - 1)) // Mask out order number of bits
	ERT_CONSTS_DEWARN;

	vi = getfn(vi, accum, 0);
	if (lvl - 1 == 0) {
		/* offset into the last level, leaf node */
		last_off = ERT_M(id, last_order) * last_sz;
	} else {
		/* calculate the offset in an internal node */
		last_off = ERT_M((id >> ((order * (lvl - 2)) + last_order)), order) * intern_sz;
	}
	return (struct ert_intern *)(((char *)vi) + last_off);
}

/*
 * This is the most optimized/most important function.
 *
 * We rely on compiler optimizations -- including constant
 * propagation, loop unrolling, function inlining, dead-code
 * elimination, and function inlining from constant function pointers
 * -- to turn this into straight-line code.  It should be on the order
 * of 10-20 instructions without loops, only including error checking
 * branches that are not taken by the static branch detection
 * algorithms.
 *
 * dlimit is the depth we should look into the tree.  This can be 0
 * (return the highest-level of the tree) all the way to depth+1 which
 * actually treats the entries in the last level of the trie as a
 * pointer and returns its destination.  dlimit = depth+1 means that the
 * size of the last-level nodes should be the size of an integer.
 */

static inline CFORCEINLINE void *
__ert_lookup(struct ert *v, unsigned long id, u32_t dstart, u32_t dlimit, void *accum, ERT_CONST_PARAMS)
{
	struct ert_intern r, *n;
	u32_t             i, limit;

	assert(v);
	assert(id < __ert_maxid(ERT_CONST_ARGS));
	assert(dlimit <= depth + 1);
	assert(dstart <= dlimit);

	/* simply gets the address of the vector */
	r.next = v->vect;
	n      = &r;
	limit  = dlimit < depth ? dlimit : depth;
	for (i = dstart; i < limit; i++) {
		if (unlikely(isnullfn(n, accum, 0))) return NULL;
		n = __ert_walk(n, id, accum, depth - i, ERT_CONST_ARGS);
	}

	if (i == depth && unlikely(!resolvefn(n, accum, 1, last_order, last_sz))) return NULL;
	if (i < depth && unlikely(!resolvefn(n, accum, 0, order, intern_sz))) return NULL;
	if (dlimit == depth + 1) n = getleaffn(n, accum);

	return n;
}

/*
 * Expand the data-structure starting from level/depth dstart, and up
 * to and including some depth limit (dlimit).  This will call the
 * initialization routines for that level, and hook it into the
 * overall trie.  If you want to control the costs of memory
 * allocation and initialization, then you should use limit to ensure
 * that multiple levels of the trie are not expanded here, if desired.
 *
 * limit == 1 does not make sense (i.e. ert is already allocated),
 * and limit = depth+1 means that we're trying to "expand" or set the
 * leaf data to something provided in the memctxt.
 */
static inline int
__ert_expand(struct ert *v, unsigned long id, u32_t dstart, u32_t dlimit, void *accum, void *memctxt, void *data,
             ERT_CONST_PARAMS)
{
	struct ert_intern r, *n, *new;
	u32_t             i, limit;

	assert(v);
	assert(id < __ert_maxid(ERT_CONST_ARGS));
	assert(dlimit <= depth + 1); /* cannot expand past leaf */
	assert(dstart <= dlimit);

	r.next = v->vect;
	n      = &r;
	limit  = dlimit < depth ? dlimit : depth;
	for (i = dstart; i < limit - 1; i++) {
		n = __ert_walk(n, id, accum, depth - i, ERT_CONST_ARGS);
		if (!isnullfn(n, accum, 0)) continue;

		/* expand via memory allocation */
		if (i + 2 < depth)
			new = allocfn(memctxt, (1 << order) * intern_sz, 0);
		else
			new = allocfn(memctxt, (1 << last_order) * last_sz, 1);

		if (unlikely(!new)) return -1;
		__ert_init(new, i + 2 >= depth, ERT_CONST_ARGS);
		setfn(n, new, accum, 0);
	}
	if (dlimit == depth + 1) {
		n = __ert_walk(n, id, accum, depth - i, ERT_CONST_ARGS);
		/* don't overwrite a value, unless we want to set it to the initval */
		if (data != initval && !isnullfn(n, accum, 0)) return 1;

		if (setleaffn(n, data)) return -ECASFAIL;
	}
	return 0;
}

#endif /* ERTRIE_H */

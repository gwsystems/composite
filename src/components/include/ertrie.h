/**
 * Copyright 2014 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef ERTRIE_H
#define ERTRIE_H

#ifndef LINUX_TEST
#include <cos_component.h>
#endif

/* Internal node in the trie */
struct ert_intern {
	struct ert_intern *next;
};
struct ert {
	struct ert_intern vect[0];
};

typedef struct ert_intern *(*ert_get_fn_t)(struct ert_intern *, unsigned long *accum, int isleaf);
typedef int  (*ert_isnull_fn_t)(struct ert_intern *, unsigned long *accum, int isleaf);
/* used to set internal initial values for nodes */
typedef void *(*ert_initval_fn_t)(struct ert_intern *, int isleaf);
typedef void (*ert_set_fn_t)(struct ert_intern *, void *accum, int isleaf);
typedef void *(*ert_alloc_fn_t)(void *data, int sz, int leaf);
typedef void (*ert_free_fn_t)(void *data, void *mem, int sz, int leaf);

static struct ert_intern *
ert_defget(struct ert_intern *a, unsigned long *accum, int isleaf)
{ (void)accum; (void)isleaf; return a; }
static int 
ert_defisnull(struct ert_intern *a, unsigned long *accum, int isleaf) 
{ (void)accum; (void)isleaf; return a == NULL; }
static void *ert_definitfn(struct ert_intern *a, int isleaf) { (void)a; (void)isleaf; return NULL; }
static void ert_defset(struct ert_intern *a, void *v, int isleaf) { (void)isleaf; a->next = v; }
static void *ert_definitval = NULL;

#define ERT_CONST_PARAMS                                             \
	u32_t depth, u32_t order, u32_t last_order, u32_t last_sz,   \
	void *initval, ert_initval_fn_t initfn, ert_get_fn_t getfn, ert_isnull_fn_t isnullfn, \
        ert_set_fn_t setfn, ert_alloc_fn_t allocfn, ert_free_fn_t freefn
#define ERT_CONST_ARGS     depth, order, last_order, last_sz, initval, initfn, getfn, isnullfn, setfn, allocfn, freefn
#define ERT_CONST_DEC_ARGS depth-1, order, last_order, last_sz, initval, initfn, getfn, isnullfn, setfn, allocfn, freefn
#define ERT_CONSTS_DEWARN  (void)depth; (void)order; (void)last_order; (void)last_sz; (void)initval, \
        (void)initfn; (void)getfn; (void)isnullfn; (void)setfn; (void)allocfn; (void)freefn;

#define ERT_CREATE(name, depth, order, last_order, last_sz, initval, initfn, getfn, isnullfn, setfn, allocfn, freefn) \
static struct ert *name##_alloc(void)					\
{ return ert_alloc(depth, order, last_order, last_sz, initval, initfn, getfn, isnullfn, setfn, allocfn, freefn); }	\
static void name##_free(struct ert *v)					\
{ ert_free(v, depth, order, last_order, last_sz, initval, initfn, getfn, isnullfn, setfn, allocfn, freefn); }	\
static inline void *name##_lkupp(struct ert *v, unsigned long id)	\
{ return ert_lkupp(v, id, depth, order, last_order, last_sz, initval, initfn, getfn, isnullfn, setfn, allocfn, freefn); } \
static inline void *name##_lkup(struct ert *v, unsigned long id)	\
{ return ert_lkup(v, id, depth, order, last_order, last_sz, initval, initfn, getfn, isnullfn, setfn, allocfn, freefn); } \
static inline void *name##_lkupa(struct ert *v, unsigned long id, unsigned long *agg)	\
{ return ert_lkupa(v, id, agg, depth, order, last_order, last_sz, initval, initfn, getfn, isnullfn, setfn, allocfn, freefn); } \
static inline unsigned long name##_maxid(void)					\
{ return (unsigned long)((1<<((order-1) * depth)) + (1<<last_order)); } \
static inline int name##_add(struct ert *v, long id, void *val)	\
{ return ert_add(v, id, val, depth, order, last_order, last_sz, initval, initfn, getfn, isnullfn, setfn, allocfn, freefn); } \
static inline int name##_del(struct ert *v, long id)			\
{ return ert_del(v, id, depth, order, last_order, last_sz, initval, initfn, getfn, isnullfn, setfn, allocfn, freefn); }

#define ERT_CREATE_DEF(name, depth, order, last_order, last_sz, allocfn, freefn)	\
ERT_CREATE(name, depth, order, last_order, last_sz, ert_definitval, ert_definitfn, ert_defget, ert_defisnull, ert_defset, allocfn, freefn)

#define ERT_MAX_ID ((unsigned long)((1<<((order-1) * depth)) + (1<<last_order)))

/* 
 * Initialize a level in the ertrie.  lvl is either an internal level,
 * lvl > 1, or a leaf level in the tree in which case embedded leaf
 * structs require a different initialization.
 */
static void
__ert_init(struct ert_intern *vi, int isleaf, ERT_CONST_PARAMS)
{
	int i, base, sz;
	ERT_CONSTS_DEWARN;

	assert(vi);
	if (!isleaf) {
		base = 1<<order;
		sz   = sizeof(int*);
	} else {
		base = 1<<last_order;
		sz   = last_sz;
	}
	for (i = 0 ; i < base ; i++) {
		struct ert_intern *t = (void*)(((char*)vi) + (sz * i));
		t->next = initfn(t, isleaf);
	}
}

static struct ert *
ert_alloc(ERT_CONST_PARAMS)
{
	struct ert *v;
	
	assert(depth > 1);
	v = allocfn(NULL, (1<<order) * sizeof(int*), 0);
	if (NULL == v) return NULL;
	__ert_init(v->vect, depth <= 1, ERT_CONST_ARGS);

	return v;
}

/* 
 * Assumes: all of the items stored in the vector have been
 * deallocated (i.e. at depth ERT_BASE, all values are set to
 * ERT_INIT_VAL).
 */
static inline void 
__ert_free_rec(struct ert_intern *vi, ERT_CONST_PARAMS)
{
	int i, sz;
	
	assert(vi);
	if (depth > 1) {
		for (i = 0 ; i < (1<<order) ; i++) {
			if (vi[i].next == NULL) continue;
			__ert_free_rec(vi[i].next, ERT_CONST_DEC_ARGS);
		}
	}
	if (depth == 1) sz = 1<<order * sizeof(int*);
	else            sz = 1<<last_order * last_sz;
	freefn(NULL, vi, sz, depth <= 1); 
}

static void 
ert_free(struct ert *v, ERT_CONST_PARAMS)
{
	assert(v);
	__ert_free_rec(v->vect, ERT_CONST_ARGS);
}

static inline struct ert_intern *
__ert_walk(struct ert_intern *vi, unsigned long id, unsigned long *accum, ERT_CONST_PARAMS)
{
	u32_t off;
	ERT_CONSTS_DEWARN;

	vi = getfn(vi->next, accum, 0);
	/* This branch should be compiled away entirely after loop
	 * unrolling and constant propagation. */
	if (depth == 0)	{
		u32_t last_off = (id & ((1<<last_order)-1)) * last_sz;
		return (struct ert_intern *)(((char *)vi) + last_off);
	}
	off = (id >> ((order * (depth-1)) + last_order)) & ((1<<order)-1);
	return &vi[off];
}

/* 
 * This is the most optimized/most important function.  
 *
 * We rely on compiler optimizations -- including constant
 * propagation, loop unrolling, function inlining, including function
 * inlining from constant function pointers -- to turn this into
 * straight-line code.  It should be on the order of 15-20
 * instructions without loops, and only with error checking branches
 * that are not taken by the static branch detection algorithms.
 */
static inline struct ert_intern *
__ert_lookup(struct ert *v, unsigned long id, unsigned long *accum, ERT_CONST_PARAMS) 
{
	struct ert_intern r, *n;

	assert(v);
	assert(depth > 0);
	assert(id < ERT_MAX_ID);
	r.next = v->vect;
	n = &r;
	for ( ; depth > 0 ; depth--) {
		n = __ert_walk(n, id, accum, ERT_CONST_DEC_ARGS);
		if (unlikely(isnullfn(n->next, accum, 0))) return NULL;
	}
	return n;
}

/* 
 * Lookup an embedded structure.
 */
static inline void *
ert_lkup(struct ert *v, unsigned long id, ERT_CONST_PARAMS)
{ 
	unsigned long agg;
	return (void*)__ert_lookup(v, id, &agg, ERT_CONST_ARGS); 
}

/* 
 * Lookup an embedded structure an aggregate over all of the getfn
 * invocations.  Only useful if you defined your own getfn, and it
 * aggregates some information about the lookup path to the final
 * value.
 */
static inline void *
ert_lkupa(struct ert *v, unsigned long id, unsigned long *agg, ERT_CONST_PARAMS)
{ return (void*)__ert_lookup(v, id, agg, ERT_CONST_ARGS); }

static int
__ert_expand(struct ert_intern *vi, unsigned long id, u32_t dlimit, ERT_CONST_PARAMS)
{
	struct ert_intern r, *n, *new;
	unsigned long accum = 0;
	u32_t i;

	assert(vi);
	assert(depth > 0);
	r.next = vi;
	n      = &r;
	for (i = 0 ; depth > 0 && i < dlimit ; depth--, i++) {
		n = __ert_walk(n, id, &accum, ERT_CONST_DEC_ARGS);
		if (!isnullfn(n->next, &accum, 0)) continue;
		
		if (depth <= 1) new = allocfn(NULL, (1<<order) * sizeof(int*), 0);
		else            new = allocfn(NULL, (1<<last_order) * last_sz, 1);
		if (unlikely(!new)) return -1;
		__ert_init(new, depth <= 1, ERT_CONST_ARGS);
		setfn(n, new, 0);
	}
	return 0;
}

/* 
 * Expand the data-structure up to some depth limit.  This will call
 * the initialization routines for that level, and hook it into the
 * overall trie.  If you want to control the costs of memory
 * allocation and initialization, then you should use dlimit to ensure
 * that multiple levels of the trie are not expanded here.
 */
static inline int
ert_expand(struct ert *v, unsigned long id, u32_t dlimit, ERT_CONST_PARAMS)
{ return __ert_expand(v->vect, id, dlimit, ERT_CONST_ARGS); }


/***
 * The following functions are only usable if the last level of the
 * trie consists of a single word (i.e. if the value is an int/long, a
 * pointer, etc...).  The specific constraint is that the last level
 * should be atomically, and fully set with a single assignment to
 * ert_initval.
 */

/* 
 * Lookup a value that is pointed to by the trie (p = ptr).  Only use
 * this function if the last_sz == sizeof a pointer.  This is enforced
 * via an assertion.  It doesn't make sense to dereference the pointer
 * in the last level of the tree if it isn't a pointer.
 */
static inline void *
ert_lkupp(struct ert *v, unsigned long id, ERT_CONST_PARAMS)
{
	struct ert_intern *vi;
	unsigned long accum;

	assert(last_sz == sizeof(int*));
	vi = __ert_lookup(v, id, &accum, ERT_CONST_ARGS);
	if (unlikely(!vi)) return NULL;
	return getfn(vi->next, &accum, 1);
}

static inline int 
__ert_set(struct ert *v, long id, void *val, ERT_CONST_PARAMS)
{
	struct ert_intern *vi;
	unsigned long accum = 0;
	
	vi = __ert_lookup(v, id, &accum, ERT_CONST_ARGS);
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
ert_add(struct ert *v, unsigned long id, void *val, ERT_CONST_PARAMS)
{
	void *p;

	assert(v);
	assert(val != initval);
	assert(id < ERT_MAX_ID);
	assert(last_sz == sizeof(int*));
	if (unlikely(ert_lkupp(v, id, ERT_CONST_ARGS))) return 1;
	if (__ert_set(v, id, val, ERT_CONST_ARGS)) {
		if (__ert_expand(v->vect, id, depth, ERT_CONST_ARGS)) return 1;
		if (__ert_set(v, id, val, ERT_CONST_ARGS))            return 1;
	}
	p = ert_lkupp(v, id, ERT_CONST_ARGS);
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
ert_del(struct ert *v, unsigned long id, ERT_CONST_PARAMS)
{
	assert(v);
	assert(id < ERT_MAX_ID);
	assert(last_sz == sizeof(int*));
	if (__ert_set(v, id, initval, ERT_CONST_ARGS)) return 1;
	assert(!ert_lkup(v, id, ERT_CONST_ARGS));

	return 0;
}

#endif /* ERTRIE_H */

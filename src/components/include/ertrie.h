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

/* get the next level/value in the internal structure/value */
typedef struct ert_intern *(*ert_get_fn_t)(struct ert_intern *, unsigned long *accum, int isleaf);
/* check if the value in the internal structure is "null" */
typedef int  (*ert_isnull_fn_t)(struct ert_intern *, unsigned long *accum, int isleaf);
/* set values to their initial value (often "null") */
typedef void *(*ert_initval_fn_t)(struct ert_intern *, int isleaf);
/* set a value in an internal structure/value */
typedef void (*ert_set_fn_t)(struct ert_intern *, void *accum, int isleaf);
/* allocate an internal or leaf structure */
typedef void *(*ert_alloc_fn_t)(void *data, int sz, int leaf);
/* ... and free it */
typedef void (*ert_free_fn_t)(void *data, void *mem, int sz, int leaf);

/* 
 * Default implementations of the customization functions that assume
 * a normal tree with pointers for internal nodes, with the "null
 * node" being equal to NULL (i.e. you can't store NULL values in the
 * structure), and setting values in internal and leaf nodes being
 * done with straightforward stores.
 */
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
#define ERT_CONSTS_DEWARN  (void)depth; (void)order; (void)last_order; (void)last_sz; (void)initval, \
        (void)initfn; (void)getfn; (void)isnullfn; (void)setfn; (void)allocfn; (void)freefn;

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
 * ~5*depth cycles (if L1 is 5 cycles to access).
 */
#define ERT_CREATE(name, depth, order, last_order, last_sz, initval, initfn, getfn, isnullfn, setfn, allocfn, freefn) \
static struct ert *name##_alloc(void *memctxt)			       \
{ return ert_alloc(memctxt, depth, order, last_order, last_sz, initval, initfn, getfn, isnullfn, setfn, allocfn, freefn); } \
static inline void *name##_lkup(struct ert *v, unsigned long id)	\
{ unsigned long a; return __ert_lookup(v, id, depth, &a, depth, order, last_order, last_sz, initval, initfn, getfn, isnullfn, setfn, allocfn, freefn); } \
static inline void *name##_lkupa(struct ert *v, unsigned long id, unsigned long *agg)  \
{ return __ert_lookup(v, id, depth, agg, depth, order, last_order, last_sz, initval, initfn, getfn, isnullfn, setfn, allocfn, freefn); } \
static inline void *name##_lkupan(struct ert *v, unsigned long id, int dlimit, unsigned long *agg) \
{ return __ert_lookup(v, id, dlimit, agg, depth, order, last_order, last_sz, initval, initfn, getfn, isnullfn, setfn, allocfn, freefn); } \
static inline int name##_expandn(struct ert *v, unsigned long id, u32_t dlimit, void *memctxt) \
{ return __ert_expand(v, id, dlimit, memctxt, depth, order, last_order, last_sz, initval, initfn, getfn, isnullfn, setfn, allocfn, freefn); } \
static inline int name##_expand(struct ert *v, unsigned long id, void * memctxt) \
{ return __ert_expand(v, id, depth, memctxt, depth, order, last_order, last_sz, initval, initfn, getfn, isnullfn, setfn, allocfn, freefn); } \
static inline unsigned long name##_maxid(void) { return (unsigned long)(1<<((order * (depth-1)) + last_order)); }

#define ERT_CREATE_DEF(name, depth, order, last_order, last_sz, allocfn, freefn)	\
ERT_CREATE(name, depth, order, last_order, last_sz, ert_definitval, ert_definitfn, ert_defget, ert_defisnull, ert_defset, allocfn, freefn)

#define ERT_MAX_ID ((unsigned long)(1<<((order * (depth-1)) + last_order)))

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
ert_alloc(void *memctxt, ERT_CONST_PARAMS)
{
	struct ert *v;
	
	/* Make sure the id size can be represented on our system */
	assert(((order * (depth-1)) + last_order) < (sizeof(unsigned long)*8));
	assert(depth > 1);
	v = allocfn(memctxt, (1<<order) * sizeof(int*), 0);
	if (NULL == v) return NULL;
	__ert_init(v->vect, depth <= 1, ERT_CONST_ARGS);

	return v;
}

static inline struct ert_intern *
__ert_walk(struct ert_intern *vi, unsigned long id, unsigned long *accum, u32_t lvl, ERT_CONST_PARAMS)
{
#define ERT_M(id, o) ((id) & ((1<<(o))-1)) // Mask out order number of bits
	ERT_CONSTS_DEWARN;

	vi = getfn(vi->next, accum, 0);
	if (lvl-1 == 0) {
		/* offset into the last level, leaf node */
		u32_t last_off = ERT_M(id, last_order) * last_sz;
		return (struct ert_intern *)(((char *)vi) + last_off);
	}
	/* calculate the offset in an internal node */
	return &vi[ERT_M((id >> ((order * (lvl-2)) + last_order)), order)];
}

/* 
 * This is the most optimized/most important function.  
 *
 * We rely on compiler optimizations -- including constant
 * propagation, loop unrolling, function inlining, dead-code
 * elimination, and including function inlining from constant function
 * pointers -- to turn this into straight-line code.  It should be on
 * the order of 15-20 instructions without loops, and only with error
 * checking branches that are not taken by the static branch detection
 * algorithms.
 */
static inline struct ert_intern *
__ert_lookup(struct ert *v, unsigned long id, u32_t dlimit, unsigned long *accum, ERT_CONST_PARAMS) 
{
	struct ert_intern r, *n;
	u32_t i, limit;

	assert(v);
	assert(id < ERT_MAX_ID);
	r.next = v->vect;
	n      = &r;
	limit  = dlimit < depth ? dlimit : depth;
	for (i = 0 ; i < limit ; i++) {
		if (unlikely(isnullfn(n->next, accum, 0))) return NULL;
		n = __ert_walk(n, id, accum, depth-i, ERT_CONST_ARGS);
	}
	return n;
}

/* 
 * Expand the data-structure up to some depth limit.  This will call
 * the initialization routines for that level, and hook it into the
 * overall trie.  If you want to control the costs of memory
 * allocation and initialization, then you should use dlimit to ensure
 * that multiple levels of the trie are not expanded here.
 */
static int
__ert_expand(struct ert *v, unsigned long id, u32_t dlimit, void *memctxt, ERT_CONST_PARAMS)
{
	struct ert_intern r, *n, *new;
	unsigned long accum = 0;
	u32_t i, limit;

	assert(v);
	assert(id < ERT_MAX_ID);
	r.next = v->vect;
	n      = &r;
	limit  = dlimit < depth ? dlimit : depth; 
	for (i = 0 ; i < limit-1 ; i++) {
		n = __ert_walk(n, id, &accum, depth-i, ERT_CONST_ARGS);
		if (!isnullfn(n->next, &accum, 0)) continue;
		
		if (i+2 < depth) new = allocfn(memctxt, (1<<order) * sizeof(int*), 0);
		else             new = allocfn(memctxt, (1<<last_order) * last_sz, 1);
		if (unlikely(!new)) return -1;
		__ert_init(new, i+2 >= depth, ERT_CONST_ARGS);
		setfn(n, new, 0);
	}
	return 0;
}

#endif /* ERTRIE_H */

/**
 * Copyright 2011 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef CVECT_H
#define CVECT_H

#ifndef CVECT_DYNAMIC
#define CVECT_DYNAMIC 1
#endif

/*
 * A simple data structure that behaves like an array in terms of
 * getting and setting, but is O(log(n)) with a base that is chose
 * below.  In most situations this will be O(log_1024(n)), or
 * essentially at most 3.  This structure has a fixed depth of
 * CVECT_DEPTH.  Default parameters yields 1024^2 = ~1 million
 * possible entries.
 *
 * The public functions are those _without_ the "__" prepended onto their
 * names:
 * CVECT_CREATE_STATIC(name) -- Create a statically allocated vect
 * void cvect_init_static(cvect_t *v) -- initialize the vect
 * void cvect_init(cvect_t *v) -- initialize any vect
 * cvect_t *cvect_alloc(void) -- dynamically allocate a vect
 * void cvect_free(cvect_t *v) -- free the vect (expensive)
 *
 * ...and the 3 common-case functions, lookup being the most optimized:
 * void *cvect_lookup(cvect_t *v, long id) -- find the stored value for id
 * void *cvect_lookup_addr(cvect_t *v, long id) -- find the address of the stored
 *                                              -- value for id
 * int cvect_add(cvect_t *v, void *val, long id) -- add a value for id
 * int cvect_del(cvect_t *v, long id) -- remove a value for id
 */

#ifdef COS_LINUX_ENV
typedef unsigned short int u16_t;
typedef unsigned int       u32_t;
#else
#include <cos_component.h>
#endif

#ifndef PAGE_SHIFT
#define PAGE_SHIFT 12
#endif
#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

/* Customization parameters: */
#ifndef CVECT_BASE
/* CVECT_BASE -- What is the fan out eat each level? Default: 1024 */
#define CVECT_BASE (PAGE_SIZE / sizeof(struct cvect_intern))
#define CVECT_SHIFT 10 /* log_2(CVECT_BASE) */
#endif

#define CVECT_SZ (CVECT_BASE * sizeof(struct cvect_intern))
#define CVECT_MASK (CVECT_BASE - 1)

#ifndef CVECT_INIT_VAL
/*
 * CVECT_INIT_VAL -- What is "empty" in the structure? Default: NULL.
 * If you're not storing pointers, this might be inappropriate.
 */
#define CVECT_INIT_VAL NULL
#endif
#ifndef CVECT_DEPTH
/* CVECT_DEPTH -- How many levels are in the structure? Default: 2 */
#define CVECT_DEPTH 2
#endif
#define CVECT_MAX_ID ((long)(CVECT_BASE * CVECT_BASE)) // really CVECT_BASE^CVECT_DEPTH

#ifdef LINUX_TEST
#include <malloc.h>
#ifndef CVECT_ALLOC
#define CVECT_ALLOC() malloc(PAGE_SIZE)
#define CVECT_FREE(x) free(x)
#endif
#else /* COS_LINUX_ENV */

#ifndef CVECT_ALLOC
/* How do we allocate and free a chunk of size CVECT_SZ? */
#define CVECT_ALLOC() alloc_page()
#define CVECT_FREE(x) free_page(x)
//#error "You must define CVECT_ALLOC and CVECT_FREE to allocate and free a page."
#endif
#endif

struct cvect_intern {
	union {
		void *               val;
		struct cvect_intern *next;
	} c;
};

typedef struct cvect_struct {
	struct cvect_intern vect[CVECT_BASE];
} cvect_t;

#define CVECT_CREATE_STATIC(name) cvect_t name = {.vect = {{.c.next = NULL}}}

/* true or false: is v a power of 2 */
static inline int
__cvect_power_2(const u32_t v)
{
	/* Assume 2's complement */
	u32_t smallest_set_bit = (v & -v);
	return (v > 1 && smallest_set_bit == v);
}

static inline int
__cvect_init(cvect_t *v)
{
	int i;

	assert(v);
	/* should be optimized away by the compiler: */
	assert(__cvect_power_2(CVECT_BASE));
	for (i = 0; i < (int)CVECT_BASE; i++) v->vect[i].c.next = NULL;

	return 0;
}

static inline void
cvect_init_static(cvect_t *v)
{
	__cvect_init(v);
}

static inline void
cvect_init(cvect_t *v)
{
	__cvect_init(v);
}

#ifdef CVECT_DYNAMIC

static cvect_t *
cvect_alloc(void)
{
	cvect_t *v;

	v = CVECT_ALLOC();
	if (NULL == v) return NULL;
	cvect_init(v);

	return v;
}

/*
 * All *_rec functions assume that constant propagation, loop
 * unrolling, and self-recursive function inlining should turn this
 * into optimal (straighline) code, yet still enable the depth and
 * vect size to be macro params.
 *
 * Regardless, this is an expensive function due to iterations through
 * a large tree.
 *
 * Assumes: all of the items stored in the vector have been
 * deallocated (i.e. at depth CVECT_BASE, all values are set to
 * CVECT_INIT_VAL).
 */
static inline void
__cvect_free_rec(struct cvect_intern *vi, const int depth)
{
	unsigned int i;

	assert(vi);
	if (depth > 1) {
		for (i = 0; i < CVECT_BASE; i++) {
			if (vi[i].c.next != NULL) {
				__cvect_free_rec(vi[i].c.next, depth - 1);
			}
		}
	}
	/* assumes "vi" is aliased with the cvect_t: */
	CVECT_FREE(vi);
}

static void
cvect_free(cvect_t *v)
{
	assert(v);
	__cvect_free_rec(v->vect, CVECT_DEPTH);
}

#endif /* CVECT_DYNAMIC */

/*
 * Again relying on compiler optimizations (constant propagation, loop
 * unrolling, and self-recursive function inlining) to turn this into
 * straight-line code.
 */
static inline struct cvect_intern *
__cvect_lookup_rec(struct cvect_intern *vi, const long id, const int depth)
{
	if (depth > 1) {
		long n = id >> (CVECT_SHIFT * (depth - 1));
		if (vi[n & CVECT_MASK].c.next == NULL) return NULL;
		return __cvect_lookup_rec(vi[n & CVECT_MASK].c.next, id, depth - 1);
	}
	return &vi[id & CVECT_MASK];
}

static inline struct cvect_intern *
__cvect_lookup(cvect_t *v, long id)
{
	return __cvect_lookup_rec(v->vect, id, CVECT_DEPTH);
}

static inline void *
cvect_lookup(cvect_t *v, long id)
{
	struct cvect_intern *vi;

	assert(v);
	assert(id >= 0);
	vi = __cvect_lookup(v, id);
	if (!vi) return NULL;
	return vi->c.val;
}

static inline void *
cvect_lookup_addr(cvect_t *v, long id)
{
	struct cvect_intern *vi;

	assert(v);
	assert(id >= 0);
	vi = __cvect_lookup(v, id);
	if (!vi) return NULL;
	return vi;
}

static inline int
__cvect_expand_rec(struct cvect_intern *vi, const long id, const int depth)
{
	if (depth > 1) {
		long n = id >> (CVECT_SHIFT * (depth - 1));
		if (vi[n & CVECT_MASK].c.next == NULL) {
			struct cvect_intern *new = CVECT_ALLOC();
			if (!new) return -1;
			memset(new, 0, PAGE_SIZE);
			vi[n & CVECT_MASK].c.next = new;
		}
		return __cvect_expand_rec(vi[n & CVECT_MASK].c.next, id, depth - 1);
	}
	return 0;
}

static inline int
__cvect_expand(cvect_t *v, long id)
{
	return __cvect_expand_rec(v->vect, id, CVECT_DEPTH);
}

static inline int
__cvect_set(cvect_t *v, long id, void *val)
{
	struct cvect_intern *vi;

	assert(v);
	vi = __cvect_lookup(v, id);
	if (NULL == vi) return -1;
	vi->c.val = val;

	return 0;
}

/*
 * This function will try to find an empty slot specifically for the
 * identifier id, or fail.
 *
 * Assume: id does not exist in v.
 */
static int
cvect_add(cvect_t *v, void *val, long id)
{
	assert(v);
	assert(val != CVECT_INIT_VAL);
	assert(id < CVECT_MAX_ID);
	assert(!cvect_lookup(v, id));
	if (__cvect_set(v, id, val)) {
		if (__cvect_expand(v, id)) return -1;
		if (__cvect_set(v, id, val)) return -1;
	}
	return 0;
}
#define cvect_add_id cvect_add /* backwards compatibility */

/*
 * Assume: id is valid within v.
 */
static int
cvect_del(cvect_t *v, long id)
{
	assert(v);
	if (__cvect_set(v, id, (void *)CVECT_INIT_VAL)) return 1;
	return 0;
}


#endif /* CVECT_H */

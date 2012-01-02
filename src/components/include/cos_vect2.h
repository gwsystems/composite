/**
 * Copyright 2011 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef COS_VECT_H
#define COS_VECT_H

#ifndef COS_VECT_DYNAMIC
#define COS_VECT_DYNAMIC 1
#endif

/* 
 * A simple data structure that behaves like an array in terms of
 * getting and setting, but is O(log(n)) with a base that is chose
 * below.  In most situations this will be O(log_1024(n)), or
 * essentially at most 3.  This structure has a fixed depth of
 * COS_VECT_DEPTH.  Default parameters yields 1024^2 = ~1 million
 * possible entries.
 *
 * The public functions are those _without_ the "__" prepended onto their
 * names:
 * COS_VECT_CREATE_STATIC(name) -- Create a statically allocated vect
 * void cos_vect_init_static(cos_vect_t *v) -- initialize the vect
 * void cos_vect_init(cos_vect_t *v) -- initialize any vect
 * cos_vect_t *cos_vect_alloc(void) -- dynamically allocate a vect
 * void cos_vect_free(cos_vect_t *v) -- free the vect (expensive)
 *
 * ...and the 3 common-case functions, lookup being the most optimized:
 * void *cos_vect_lookup(cos_vect_t *v, long id) -- find the stored value for id
 * int cos_vect_add(cos_vect_t *v, void *val, long id) -- add a value for id
 * int cos_vect_del(cos_vect_t *v, long id) -- remove a value for id
 */

#ifdef COS_LINUX_ENV
typedef unsigned short int u16_t;
typedef unsigned int u32_t;
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
#ifndef COS_VECT_BASE
/* COS_VECT_BASE -- What is the fan out eat each level? Default: 1024 */
#define COS_VECT_BASE  (PAGE_SIZE/sizeof(struct cos_vect_intern))
#define COS_VECT_SHIFT 10 	/* log_2(COS_VECT_BASE) */
#endif

#define COS_VECT_SZ   (COS_VECT_BASE*sizeof(struct cos_vect_intern))
#define COS_VECT_MASK  (COS_VECT_BASE-1)

#ifndef COS_VECT_INIT_VAL
/* 
 * COS_VECT_INIT_VAL -- What is "empty" in the structure? Default: NULL.
 * If you're not storing pointers, this might be inappropriate.
 */
#define COS_VECT_INIT_VAL NULL
#endif
#ifndef COS_VECT_DEPTH
/* COS_VECT_DEPTH -- How many levels are in the structure? Default: 2 */
#define COS_VECT_DEPTH 2
#endif

#ifdef COS_LINUX_ENV
#include <malloc.h>
#ifndef COS_VECT_ALLOC
#define COS_VECT_ALLOC() malloc(PAGE_SIZE)
#define COS_VECT_FREE(x)  free(x)
#endif
#else  /* COS_LINUX_ENV */
#include <cos_alloc.h>
#ifndef COS_VECT_ALLOC
/* How do we allocate and free a chunk of size COS_VECT_SZ? */
#define COS_VECT_ALLOC() alloc_page()
#define COS_VECT_FREE(x) free_page(x)
#endif
#endif


struct cos_vect_intern {
	union {
		void *val;
		struct cos_vect_intern *next;
	} c;
};

typedef struct cos_vect_struct {
	struct cos_vect_intern vect[COS_VECT_BASE];
} cos_vect_t;

#define COS_VECT_CREATE_STATIC(name)					\
	cos_vect_t name = {.vect = {{.c.next = NULL}}} 

/* true or false: is v a power of 2 */
static inline int 
__cos_vect_power_2(const u32_t v)
{
	/* Assume 2's complement */
	u32_t smallest_set_bit = (v & -v);
	return (v > 1 && smallest_set_bit == v);
}

static inline int 
__cos_vect_init(cos_vect_t *v)
{
	int i;

	assert(v);
	/* should be optimized away by the compiler: */
	assert(__cos_vect_power_2(COS_VECT_BASE));
	for (i = 0 ; i < (int)COS_VECT_BASE ; i++) v->vect[i].c.next = NULL;

	return 0;
}

static inline void 
cos_vect_init_static(cos_vect_t *v)
{
	__cos_vect_init(v);
}

static inline void 
cos_vect_init(cos_vect_t *v)
{
	__cos_vect_init(v);
}

#ifdef COS_VECT_DYNAMIC

static cos_vect_t *
cos_vect_alloc(void)
{
	cos_vect_t *v;
	
	v = COS_VECT_ALLOC();
	if (NULL == v) return NULL;
	cos_vect_init(v);

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
 * deallocated (i.e. at depth COS_VECT_BASE, all values are set to
 * COS_VECT_INIT_VAL).
 */
static inline void 
__cos_vect_free_rec(struct cos_vect_intern *vi, const int depth)
{
	unsigned int i;
	
	assert(vi);
	if (depth > 1) {
		for (i = 0 ; i < COS_VECT_BASE ; i++) {
			if (vi[i].c.next != NULL) {
				__cos_vect_free_rec(vi[i].c.next, depth-1);
			}
		}
	}
	/* assumes "vi" is aliased with the cos_vect_t: */
	COS_VECT_FREE(vi);
}

static void 
cos_vect_free(cos_vect_t *v)
{
	assert(v);
	__cos_vect_free_rec(v->vect, COS_VECT_DEPTH);
}

#endif /* COS_VECT_DYNAMIC */

/* 
 * Again relying on compiler optimizations (constant propagation, loop
 * unrolling, and self-recursive function inlining) to turn this into
 * straight-line code.
 */
static inline struct cos_vect_intern *
__cos_vect_lookup_rec(struct cos_vect_intern *vi, const long id, const int depth)
{
	if (depth > 1) {
		long n = id >> (COS_VECT_SHIFT * (depth-1));
		if (vi[n & COS_VECT_MASK].c.next == NULL) return NULL;
		return __cos_vect_lookup_rec(vi[n & COS_VECT_MASK].c.next, id, depth-1);
	}
	return &vi[id & COS_VECT_MASK];
}

static inline struct cos_vect_intern *
__cos_vect_lookup(cos_vect_t *v, long id) 
{ 
	return __cos_vect_lookup_rec(v->vect, id, COS_VECT_DEPTH); 
}

static inline void *
cos_vect_lookup(cos_vect_t *v, long id)
{
	struct cos_vect_intern *vi;

	assert(v);
	assert(id >= 0);
	vi = __cos_vect_lookup(v, id);
	if (!vi) return NULL;
	return vi->c.val;
}

static inline int
__cos_vect_expand_rec(struct cos_vect_intern *vi, const long id, const int depth)
{
	if (depth > 1) {
		long n = id >> (COS_VECT_SHIFT * (depth-1));
		if (vi[n & COS_VECT_MASK].c.next == NULL) {
			struct cos_vect_intern *new = COS_VECT_ALLOC();
			if (!new) return -1;
			vi[n & COS_VECT_MASK].c.next = new;
		}
		return __cos_vect_expand_rec(vi[n & COS_VECT_MASK].c.next, id, depth-1);
	}
	return 0;
}

static inline int 
__cos_vect_expand(cos_vect_t *v, long id)
{
	return __cos_vect_expand_rec(v->vect, id, COS_VECT_DEPTH);
}

static inline int 
__cos_vect_set(cos_vect_t *v, long id, void *val)
{
	struct cos_vect_intern *vi;
	
	assert(v);
	vi = __cos_vect_lookup(v, id);
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
cos_vect_add(cos_vect_t *v, void *val, long id)
{
	assert(v && val != COS_VECT_INIT_VAL);
	assert(!cos_vect_lookup(v, id));
	if (__cos_vect_set(v, id, val)) {
		if (__cos_vect_expand(v, id)) return -1;
		if (__cos_vect_set(v, id, val)) return -1;
	}
	return 0;
}
#define cos_vect_add_id cos_vect_add /* backwards compatibility */

/* 
 * Assume: id is valid within v.
 */
static int 
cos_vect_del(cos_vect_t *v, long id)
{
	assert(v);
	if (__cos_vect_set(v, id, (void*)COS_VECT_INIT_VAL)) return 1;
	return 0;
}


#endif /* COS_VECT_H */

/***
 * Copyright 2011-2015 by Gabriel Parmer.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gparmer@gwu.edu, 2011
 *
 * History:
 * - Initial slab allocator, 2011
 * - Adapted for parsec, 2015
 */

#ifndef  PS_SLAB_H
#define  PS_SLAB_H

/* #include <cos_component.h> */
/* #include <cos_debug.h> */
/* #include <consts.h> */

#include <ps_list.h>
#include <ps_plat.h>

struct ps_mheader;

/* The header for a slab. */
struct ps_slab {
	void *memory;		/* != NULL iff slab is separately allocated */
	u32_t memsz;		/* size of backing memory */
	u32_t obj_sz;		/* size of the objects in slab */
	u16_t coreid;		/* which is the home core for this slab? */
	u16_t nfree; 		/* # allocations in freelist */
	struct ps_mheader *freelist; /* free objs in this slab */

	struct ps_list list;    /* freelist of slabs */
} PS_PACKED;

/* Memory header */
struct ps_mheader {
	u64_t              tsc_free;
	struct ps_slab    *slab;	       /* slab header ptr */
	struct ps_mheader *next;	       /* slab freelist ptr */
} PS_PACKED;

struct ps_slab_freelist {
	struct ps_slab *list;
};

/*
 * TODO:
 * 1. save memory by packing multiple freelists into the same
 * cache-line
 * 2. have multiple freelists (e.g. 4) for different "fullness"
 * values, so that we can in O(1) always allocate from the slab that
 * is most full, modulo the granularity of these bins.
 * 3. implement the slab to allocate the headers for the other slab.
 *
 * Note the padding is for two cache-lines due to the observed
 * behavior on Intel chips to aggressively prefetch an additional
 * cache-line.
 */
struct ps_slab_freelist_clpad {
	struct ps_slab_freelist fl;
	struct ps_slab_freelist slabheads;
	char padding[PS_CACHE_LINE*2-sizeof(struct ps_slab_freelist)*2];
} PS_PACKED PS_ALIGNED;

/*** Utility operations on the data-structures ***/

static inline struct ps_mheader *
__ps_mhead_get(void *mem)
{ return (struct ps_mheader *)((char*)mem - sizeof(struct ps_mheader)); }

static inline int
__ps_mhead_isfree(struct ps_mheader *h)
{ return h->tsc_free != 0; }

static inline struct ps_mheader *
__ps_mhead_flnext(struct ps_mheader *h)
{
	if (!__ps_mhead_isfree(h)) return NULL;
	return h->next;
}

static inline struct ps_slab *
__ps_mhead_slab(struct ps_mheader *h)
{ return h->slab; }

static inline void
__ps_mhead_init(struct ps_mheader *h, struct ps_slab *s)
{
	h->tsc_free = 0;
	h->slab     = s;
	h->next     = NULL;
}

static inline void
__ps_mhead_reset(struct ps_mheader *h)
{
	h->tsc_free = 0;
	h->next     = NULL;
}

/* If you don't need memory anymore, set it free! */
static inline struct ps_slab *
__ps_mhead_setfree(struct ps_mheader *h)
{
	h->tsc_free  = ps_tsc() | 1; /* guarantee non-zero */

	return h->slab;
}

/* These functions should be statically computed... */
static inline unsigned int
__ps_slab_objmemsz(size_t obj_sz)
{ return PS_RNDUP(obj_sz + sizeof(struct ps_mheader), PS_WORD); }
static inline unsigned int
__ps_slab_max_nobjs(size_t obj_sz, size_t allocsz, int hintern)
{ return (allocsz - (hintern ? sizeof(struct ps_slab) : 0)) / __ps_slab_objmemsz(obj_sz); }

/*** Operations on the freelist of slabs ***/

static void
__slab_freelist_rem(struct ps_slab_freelist *fl, struct ps_slab *s)
{
	assert(s && fl);
	if (fl->list == s) {
		if (ps_list_empty(s, list)) fl->list = NULL;
		else                        fl->list = ps_list_first(s, list);
	}
	ps_list_rem(s, list);
}

static void
__slab_freelist_add(struct ps_slab_freelist *fl, struct ps_slab *s)
{
	assert(s && fl);
	assert(ps_list_empty(s, list));
	assert(s != fl->list);
	if (fl->list) ps_list_add(fl->list, s, list);
	fl->list = s;
	/* TODO: sort based on emptiness...just use N bins */
}

/*** Alloc and free ***/

static inline void
__ps_slab_mem_free(void *buf, struct ps_slab_freelist_clpad *fls, size_t obj_sz, size_t allocsz, int hintern)
{
	struct ps_slab *s;
	struct ps_mheader *h, *next;
	unsigned int max_nobjs = __ps_slab_max_nobjs(obj_sz, allocsz, hintern);
	/* TODO: struct ps_slab_freelist *headfl; */
	struct ps_slab_freelist *fl;
	assert(obj_sz + (hintern ? sizeof(struct ps_slab) : 0) <= allocsz);

	h = __ps_mhead_get(buf);
	assert(!__ps_mhead_isfree(h));
	s = __ps_mhead_setfree(h);
	assert(s);

	next        = s->freelist;
	s->freelist = h; 	/* TODO: should be atomic/locked */
	h->next     = next;
	s->nfree++;		/* TODO: ditto */

	if (s->nfree == max_nobjs) {
		/* remove from the freelist */
		fl = &fls[s->coreid].fl;
		__slab_freelist_rem(fl, s);
	 	PS_SLAB_FREE(s, s->memsz);
	} else if (s->nfree == 1) {
		fl = &fls[s->coreid].fl;
		/* add back onto the freelists */
		assert(ps_list_empty(s, list));
		__slab_freelist_add(fl, s);
	}

	return;
}

static void
__ps_slab_init(struct ps_slab *s, struct ps_slab_freelist *fl, size_t obj_sz, int allocsz, int hintern)
{
	size_t nfree, i;
	size_t start_off = sizeof(struct ps_slab) * hintern; /* hintern \in {0, 1}*/
	size_t objmemsz  = __ps_slab_objmemsz(obj_sz);
	struct ps_mheader *alloc, *prev;
	void *u = s; 		/* untyped slab, for memory arithmetic */

	assert(hintern == 0 || hintern == 1);
	/* division should be statically calculated with enough inlining */
	s->nfree  = nfree = (allocsz - start_off) / objmemsz;
	s->obj_sz = obj_sz;
	s->memsz  = allocsz;
	s->memory = s;
	s->coreid = ps_coreid();

	/*
	 * Set up the slab's freelist
	 *
	 * TODO: cache coloring
	 */
	alloc     = (struct ps_mheader *)((char *)u + start_off);
	prev      = s->freelist = alloc;
	for (i = 0 ; i < nfree ; i++, prev = alloc, alloc = (struct ps_mheader *)((char *)alloc + objmemsz)) {
		__ps_mhead_init(alloc, s);
		prev->next = alloc;
	}
	/* better not overrun memory */
	assert((void *)alloc <= (void *)((char*)s + allocsz));

	ps_list_init(s, list);
	__slab_freelist_add(fl, s);
}

static inline void *
__ps_slab_mem_alloc(struct ps_slab_freelist *fl, size_t obj_sz, u32_t allocsz, int hintern, struct ps_slab_freelist *headfl)
{
	struct ps_slab *s;
	struct ps_mheader *h;
	assert(obj_sz + (hintern ? sizeof(struct ps_slab) : 0) <= allocsz);
	(void)headfl;

	s = fl->list;
	if (unlikely(!s)) {
		assert(hintern);
		s = PS_SLAB_ALLOC(allocsz);
		if (unlikely(!s)) return NULL;
		__ps_slab_init(s, fl, obj_sz, allocsz, hintern);
	}

	/* TODO: atomic modification to the freelist */
	h           = s->freelist;
	s->freelist = h->next;
	h->next     = NULL;
	s->nfree--;
	__ps_mhead_reset(h);
	/* remove from the freelist */
	if (s->nfree == 0) __slab_freelist_rem(fl, s);
	assert(!__ps_mhead_isfree(h));

	return &h[1];
}

/***
 * This macro is very important for high-performance.  It creates the
 * functions for allocation and deallocation passing in the freelist
 * directly, and size information for these objects, thus enabling the
 * compiler to do partial evaluation.  This avoids freelist lookups,
 * and relies on the compilers optimizations to generate specialized
 * code for the given sizes -- requires function inlining and constant
 * propagation.  Relying on these optimizations is better than putting
 * all of the code for allocation and deallocation in the macro due to
 * maintenance and readability.
 */
#define PS_SLAB_CREATE_DATA(name, size)					\
struct ps_slab_freelist_clpad slab_##name##_freelist[PS_NUMCORES] PS_ALIGNED;

#define PS_SLAB_CREATE_FNS(name, size, allocsz, headintern)		\
inline void *						                \
ps_slab_alloc_##name(void)						\
{									\
        struct ps_slab_freelist_clpad *fl = &slab_##name##_freelist[ps_coreid()]; \
	return __ps_slab_mem_alloc(&fl->fl, size, allocsz, headintern, &fl->slabheads); \
}									\
inline void							        \
ps_slab_free_##name(void *buf)						\
{									\
        struct ps_slab_freelist_clpad *fl = slab_##name##_freelist;	\
	__ps_slab_mem_free(buf, fl, size, allocsz, headintern);	        \
}									\
inline size_t								\
ps_slab_objmem_##name(void)						\
{ return __ps_slab_objmemsz(size); }					\
inline size_t								\
ps_slab_nobjs_##name(void)						\
{ return __ps_slab_max_nobjs(size, allocsz, headintern); }

/*
 * allocsz is the size of the backing memory allocation, and
 * headintern is 0 or 1, should the ps_slab header be internally
 * allocated from that slab of memory, or from elsewhere.
 *
 * Note: if you use headintern == 1, then you must manually create
 * PS_SLAB_CREATE_DEF(meta, sizeof(struct ps_slab));
 */
#define PS_SLAB_CREATE(name, size, allocsz, headintern)	\
PS_SLAB_CREATE_DATA(name, size)				\
PS_SLAB_CREATE_FNS(name, size, PS_PAGE_SIZE, headintern)

#define PS_SLAB_CREATE_DEF(name, size)		\
PS_SLAB_CREATE(name, size, PS_PAGE_SIZE, 1)

/* Create function prototypes for cross-object usage */
#define PS_SLAB_CREATE_PROTOS(name)				\
void  *ps_slab_alloc_##name(void);			        \
void   ps_slab_free_##name(void *buf);				\
size_t ps_slab_objmem_##name(void);                             \
size_t ps_slab_nobjs_##name(void);

#endif /* PS_SLAB_H */

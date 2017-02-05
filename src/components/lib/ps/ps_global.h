/***
 * Copyright 2011-2015 by Gabriel Parmer.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gparmer@gwu.edu, 2011
 */

#ifndef PS_GLOBAL_H
#define PS_GLOBAL_H

#include <ps_plat.h>

typedef unsigned long ps_desc_t;

/*
 * Lists of free memory.  The slab freelist is all slabs that have at
 * least one free object in them.  The qsc_list is a quiescence list
 * of memory that has been freed, but might still have references to
 * it (ala parsec).
 */
struct ps_slab;
struct ps_slab_freelist {
	struct ps_slab    *list;
};

typedef ps_tsc_t ps_free_token_t;
/* Memory header */
struct ps_mheader {
	ps_free_token_t    tsc_free;
	struct ps_slab    *slab;	       /* slab header ptr */
	struct ps_mheader *next;	       /* slab freelist ptr */
} PS_PACKED;

static inline struct ps_mheader *
__ps_mhead_get(void *mem)
{ return (struct ps_mheader *)((char*)mem - sizeof(struct ps_mheader)); }

static inline void *
__ps_mhead_mem(struct ps_mheader *h)
{ return &h[1]; }

static inline int
__ps_mhead_isfree(struct ps_mheader *h)
{ return h->tsc_free != 0; }

static inline void
__ps_mhead_reset(struct ps_mheader *h)
{
	h->tsc_free = 0;
	h->next     = NULL;
}

/* If you don't need memory anymore, set it free! Assumes: token != 0*/
static inline void
__ps_mhead_setfree(struct ps_mheader *h, ps_free_token_t token)
{
	/* TODO: atomic w/ error out */
	h->tsc_free = token; /* Assumption: token must be guaranteed to be non-zero */
}

static inline void
__ps_mhead_init(struct ps_mheader *h, struct ps_slab *s)
{
	h->slab     = s;
	__ps_mhead_setfree(h, 1);
}

struct ps_qsc_list {
	struct ps_mheader *head, *tail;
};

static inline struct ps_mheader *
__ps_qsc_peek(struct ps_qsc_list *ql)
{ return ql->head; }

static inline void
__ps_qsc_enqueue(struct ps_qsc_list *ql, struct ps_mheader *n)
{
	struct ps_mheader *t;

	t = ql->tail;
	if (likely(t)) t->next  = ql->tail = n;
	else           ql->head = ql->tail = n;
}

static inline struct ps_mheader *
__ps_qsc_dequeue(struct ps_qsc_list *ql)
{
	struct ps_mheader *a = ql->head;

	if (a) {
		ql->head = a->next;
		if (unlikely(ql->tail == a)) ql->tail = NULL;
		a->next = NULL;
	}
	return a;
}

static inline struct ps_mheader *
__ps_qsc_clear(struct ps_qsc_list *l)
{
	struct ps_mheader *m = l->head;

	l->head = l->tail = NULL;

	return m;
}

struct ps_slab_remote_list {
	struct ps_lock     lock;
	struct ps_qsc_list remote_frees;
	size_t             nfree;
} PS_ALIGNED;

struct ps_slab_info {
	struct ps_slab_freelist fl;	      /* freelist of slabs with available objects */
	unsigned long           salloccnt;    /* # of times we've allocated, used to batch remote dequeues */
	unsigned long           remote_token; /* which of the remote NUMA nodes will we dequeue from this time? */
	unsigned long           nslabs;       /* # of slabs allocated here */
};

struct parsec;
struct ps_smr_info {
	struct parsec     *ps;         /* the parallel section that wraps this memory, or NULL */
	struct ps_qsc_list qsc_list;   /* queue of freed, but not quiesced memory */
	size_t             qmemcnt;    /* # of items in the qsc_list */
	size_t             qmemtarget; /* # of items in qsc_list before we attempt to quiesce */
};

typedef void *(*ps_lkupan_fn_t)(void *v, unsigned long id, u32_t dlimit, void *accum);
typedef int   (*ps_expand_fn_t)(void *v, unsigned long id, u32_t dlimit, void *accum, void *memctxt, void *data);

struct ps_ns_info {
	void *ert;
	size_t ert_depth;
	ps_lkupan_fn_t lkupfn;
	ps_expand_fn_t expandfn;

	size_t desc_range; 	/* num descriptors per slab */
	ps_desc_t desc_max;
	char  padding[PS_CACHE_PAD-(3*sizeof(void *)+2*sizeof(size_t)+sizeof(ps_desc_t))%PS_CACHE_PAD];

	struct ps_lock lock;
	struct ps_slab_freelist fl;
	ps_desc_t frontier;
	char  padding2[PS_CACHE_PAD-(sizeof(struct ps_lock) + sizeof(ps_desc_t) + sizeof(struct ps_slab_freelist))];
} PS_PACKED PS_ALIGNED;

/*
 * TODO:
 * 1. save memory by packing multiple freelists into the same
 * cache-line
 * 2. have multiple freelists (e.g. 4) for different "fullness"
 * values, so that we can in O(1) always allocate from the slab that
 * is most full, modulo the granularity of these bins.
 * 3. implement the slab to allocate the headers for the other slab.
 *
 * Note: some of these TODOs are more applicable to the
 * ps_slab_freelist.
 *
 * Note: the padding is for two cache-lines due to the observed
 * behavior on Intel chips to aggressively prefetch an additional
 * cache-line.
 */
struct ps_mem_percore {
	struct ps_slab_info slab_info;
	struct ps_smr_info  smr_info;

	char padding[PS_CACHE_PAD-((sizeof(struct ps_slab_info) + sizeof(struct ps_smr_info))%PS_CACHE_PAD)];

	/* 
	 * Isolate the contended cache-lines from the common-case
	 * ones, maintain a set of them per numa node to ensure that
	 * performing remote frees does not contend across more than 2
	 * numa nodes (this node, and the destination node).
	 */
	struct ps_slab_remote_list slab_remote[PS_NUMLOCALITIES] PS_ALIGNED;
} PS_ALIGNED;

struct ps_locality_info {
	localityid_t core_locality[PS_NUMCORES];
} PS_ALIGNED;

struct ps_mem {
	struct ps_ns_info       ns_info;
	struct ps_mem_percore   percore[PS_NUMCORES];	
} PS_ALIGNED;

#define __PS_MEM_CREATE_DATA(name) struct ps_mem __ps_mem_##name;

typedef void            (*ps_free_fn_t)(struct ps_mem *m, struct ps_slab *s, size_t sz, coreid_t curr);
typedef struct ps_slab *(*ps_alloc_fn_t)(struct ps_mem *m, size_t sz, coreid_t curr);

#endif	/* PS_GLOBAL_H */

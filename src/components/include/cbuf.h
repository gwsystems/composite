/**
 * Copyright 2010 by The George Washington University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gparmer@gwu.edu, 2010
 * Updated by Qi Wang and Jiguo Song, 2011
 * Updated and simplified by removing sub-page allocations, Gabe Parmer, 2012
 */

#ifndef  CBUF_H
#define  CBUF_H

#include <cos_component.h>
#include <cos_debug.h>
#include <cbuf_c.h>
#include <cbuf_vect.h>
#include <cos_vect.h>
#include <cos_list.h>
#include <bitmap.h>
#include <cos_synchronization.h>

#include <cos_alloc.h>
#define CVECT_ALLOC() alloc_page()
#define CVECT_FREE(x) free_page(x)
#include <cvect.h>

#include <tmem_conf.h>

extern cos_lock_t cbuf_lock;
#define CBUF_TAKE()    do { if (unlikely(cbuf_lock.lock_id == 0)) lock_static_init(&cbuf_lock); if (unlikely(lock_take_up(&cbuf_lock))) BUG(); } while(0)
#define CBUF_RELEASE() do { if (unlikely(lock_release_up(&cbuf_lock))) BUG(); } while(0)
//#define CBUF_TAKE()
//#define CBUF_RELEASE()

/* 
 * Shared buffer management for Composite.
 *
 * A delicate dance between cbuf structures.  There are a number of
 * design goals: 1) cbuf_t must be passed between components without
 * shared memory (i.e. it must fit in a register).  2) each component
 * should be able to efficiently allocate memory for a given principal
 * (i.e. thd) that can be accessed by that principal in other
 * components (but not necessarily written).  3) Each component should
 * be able to efficiently and predictably find a buffer referenced by
 * a cbuf_t.  These are brought in to the component on-demand, i.e. if
 * a component has already accessed the page that holds the cbuf, then
 * it will be able to find it only from its own "cache"; however, if
 * it hasn't, it will need to bring in that buffer from another
 * component using shared memory.  Locality of access will ensure that
 * this is the uncommon case.  4) A component given a cbuf_t should
 * not have a chance of crashing when accessing the buffer.  This
 * implies that the cbuf_t does not contain any information that can't
 * be verified as correct from the component's own data-structures.
 *
 * A large assumption I'm making is that buffers can be fragmented and
 * spread around memory.  This relies on a programming model that is
 * scatter-gather.  This assumption enables all buffers to be less
 * than or equal to the size of a page.  Larger "buffers" are a
 * collection of multiple smaller buffers.
 *
 * Given these constraints, the solution we take follows:
 *
 * 1) cbuf_t is 32 bits, and includes a) the id associated with the
 * page containing the buffer.  This id is assigned by a trusted
 * component, and the namespace is shared between components (i.e. the
 * id is meaningful and is the identify mapping between components),
 * and b) the offset into the page for the desired object.  This
 * latter offset is not the byte-offset.  Instead each page is "typed"
 * in that it holds objects only of a certain size, and more
 * specifically, and array of objects of a specific size.  The offset,
 * is the index into this array.
 *
 * 2) cbuf_meta is a data-structure that is stored per-component, thus
 * it is trusted by the component to which it belongs.  A combination
 * of the (principal_id, cbuf page id) is used to lookup the
 * appropriate referenced cbuf_meta structure, and it also fits in 32
 * bits so that it can be held directly in the index structure (it's
 * lookup structure).  It holds a) a pointer to the page in this
 * component, and b) the size of objects held in this page.
 *
 * 3) Any references to buffers via interface invocations pass both
 * the cbuf_t, and the length of the allocation, if it is less than
 * the size associated with the page.  
 *
 * With these three pieces of data we satisfy the constraints because
 * 1) cbuf_t is 32 bits and is passed in a register between
 * components, 2) a component can allocate its own buffers, and share
 * their cid_t with others, 3) cbuf_meta can be found efficiently
 * using cbuf_t, and we get the buffer pointer from the cbuf_meta's
 * data, and the length of the buffer from the argument to the
 * interface function, 4) cbuf_meta information is trusted and not
 * corruptible by other components, and though it is found using
 * information in the untrusted cbuf_t, that data is verified in the
 * cbuf_meta lookup process (i.e. does the page id exist, does the it
 * belong to the principal, etc...).
 */

/***
 * Concurrency invariants:
 * 
 * 1) While the cbuf lock is taken, only this thread will access the
 * slab and set the USED bits in the meta structure.  Keep in mind
 * that many of the functions that call the cbuf manager interface
 * functions do drop this lock, thus requiring us to re-check the
 * state of these data-structures.
 *
 * 2) While the USED bit is set in a meta entry, the cbuf manager will
 * not remove the cbuf from us and from the meta-entry.  Therefore,
 * setting the USED bit acts as a form of a lock between this
 * component and the manager.
 */

/***
 * On data-structure consistency:
 *
 * The cbuf_meta data-structure is shared between manager and client,
 * and is one-to-one mapped with the cbufs themselves.  These are
 * indexed by the cbuf id.  The cbuf_alloc_descs are a client-local
 * data-structure tracking the cbuf allocations (and caching them).
 * These are indexed by the address of the cbuf (to enable cbuf_free),
 * and stored in local free-lists.
 *
 * When the USED bit is not set in the cbuf_meta structure (i.e. it is
 * in the common case on a free-list in the client), the manager can
 * remove it at any time.  This means that free-list entries can exist
 * that don't actually refer to a cbuf.  We call these stale free-list
 * entries (see __cbuf_alloc_meta_inconsistent for the check to see if
 * this is the case) that must be cleaned up.
 *
 * The more difficult case is this: 1) the manager removes a cbuf from
 * the client (but the free-list entry still remains), 2) a thread t_0
 * attempts to allocate a cbuf and must ask the manager for a new cbuf
 * which is then mapped into the client and added to the cbuf_meta
 * structure (with the USED bit set) at the same address of the old
 * cbuf that was removed, and 3) before it can return, another thread
 * t_1 enters the cbuf code and finds the stale free-list entry.  The
 * question here is this: How does t_1 know that the free-list entry is
 * stale, and that it should be deallocated?  Answer: If a cbuf is
 * marked as USED when it is seen to be on the free-list, we know this
 * case has been triggered.  
 *
 * Similar to this latter case, when a thread allocates a new cbuf
 * (cbuf_c_create), it should check to see if a freelist descriptor
 * exists before allocating a new one to avoid two descriptors
 * existing for one cbuf.
 *
 * Luckily, the correct way to fix all these inconsistencies is the
 * same: deallocate the allocation free-list descriptor.
 */

typedef u32_t cbuf_t; /* Requirement: gcc will return this in a register */
typedef union {
	cbuf_t v;
	struct {
		u32_t id:20, len:12; /* cbuf id and its maximum length */
	} __attribute__((packed)) c;
} cbuf_unpacked_t;

static inline void 
cbuf_unpack(cbuf_t cb, u32_t *cbid, u32_t *len) 
{
	cbuf_unpacked_t cu = {0};
	
	cu.v  = cb;
	*cbid = cu.c.id;
	*len  = (u32_t)cu.c.len;
	return;
}

static inline cbuf_t 
cbuf_cons(u32_t cbid, u32_t len) 
{
	cbuf_unpacked_t cu;
	cu.c.id  = cbid;
	cu.c.len = len;
	return cu.v; 
}

static inline cbuf_t cbuf_null(void)      { return 0; }
static inline int cbuf_is_null(cbuf_t cb) { return cb == 0; }

extern struct cbuf_alloc_desc *__cbuf_alloc_slow(int size, int *len);
extern int __cbuf_2buf_miss(int cbid, int len);
extern void __cbuf_desc_free(struct cbuf_alloc_desc *d);
extern cvect_t meta_cbuf;

/* 
 * Common case.  This is the most optimized path.  Every component
 * that wishes to access a cbuf created by another component must use
 * this function to map the cbuf_t to the actual buffer.
 */
static inline void * 
cbuf2buf(cbuf_t cb, int len)
{
	int sz;
	u32_t id;
	struct cbuf_meta *cm;
	union cbufm_info ci;//, ci_new;
	void *ret = NULL;
	long cbidx;
	if (unlikely(!len)) return NULL;
	cbuf_unpack(cb, &id, (u32_t*)&sz);

	CBUF_TAKE();
	cbidx = cbid_to_meta_idx(id);
again:
	do {
		cm = cbuf_vect_lookup_addr(&meta_cbuf, cbidx);
		if (unlikely(!cm || cm->nfo.v == 0)) {
			if (__cbuf_2buf_miss(id, len)) goto done;
			goto again;
		}
	} while (unlikely(!cm->nfo.v));
	ci.v = cm->nfo.v;

	/* if (unlikely(cm->sz && (((int)cm->sz)<<PAGE_ORDER) < len)) goto done; */
	/* ci_new.v     = ci.v; */
	/* ci_new.c.flags = ci.c.flags | CBUFM_RECVED | CBUFM_IN_USE; */
	/* if (unlikely(!cos_cas((unsigned long *)&cm->nfo.v,  */
	/* 		      (unsigned long)   ci.v,  */
	/* 		      (unsigned long)   ci_new.v))) goto again; */
	ret = ((void*)(cm->nfo.c.ptr << PAGE_ORDER));
done:	
	CBUF_RELEASE();
	assert(lock_contested(&cbuf_lock) != cos_get_thd_id());
	return ret;
}

extern cvect_t alloc_descs; 
struct cbuf_alloc_desc {
	int cbid, length;
	void *addr;
	struct cbuf_meta *meta;
	struct cbuf_alloc_desc *next, *prev, *flhead; /* freelist */
};
extern struct cbuf_alloc_desc cbuf_alloc_freelists;

static inline struct cbuf_alloc_desc *
__cbuf_alloc_lookup(int page_index) { return cvect_lookup(&alloc_descs, page_index); }

/* 
 * Assume that m was retrieved with 
 * m = cbuf_vect_lookup_addr(&meta_cbuf, cbid_to_meta_idx(d->cbid));
 * This validates that the d->cbid = cbid of cbuf_meta.
 */
static inline int
__cbuf_alloc_meta_inconsistent(struct cbuf_alloc_desc *d, struct cbuf_meta *m)
{
	assert(d && m && d->addr);
	/* we don't want the manager changing this under us */
	assert(m->nfo.c.flags & CBUFM_IN_USE);
	return (unlikely((unsigned long)d->addr >> PAGE_SHIFT != m->nfo.c.ptr ||
			 d->meta != m /*|| length*/));
}

static inline void *
cbuf_alloc(unsigned int sz, cbuf_t *cb)
{
	void *ret;
	struct cbuf_alloc_desc *d;
	int cbid, len, already_used, mapped_in;
	struct cbuf_meta *cm;
	long cbidx;

	CBUF_TAKE();
again:
	d = FIRST_LIST(&cbuf_alloc_freelists, next, prev);
	if (unlikely(EMPTY_LIST(d, next, prev))) {
		d    = __cbuf_alloc_slow(sz, &len);
		assert(d);
		ret  = d->addr;
		cbid = d->cbid;
		goto done;
		//TODO: check if this cbuf has been taken by another thd already.
		// shall we add this cbuf to the freelist and just continue?
	} 
	cbid  = d->cbid;
	REM_LIST(d, next, prev);
	assert(EMPTY_LIST(d, next, prev));
	assert(cbid);
	cbidx            = cbid_to_meta_idx(cbid);
	cm               = cbuf_vect_lookup_addr(&meta_cbuf, cbidx);

	mapped_in        = cbufm_is_mapped(cm);
	already_used     = cm->nfo.c.flags & CBUFM_IN_USE;
	cm->nfo.c.flags |= CBUFM_IN_USE | CBUFM_TOUCHED; /* should be atomic */

	/* 
	 * Now that IN_USE is set, we know the manager will not rip
	 * this out from under us.  Check that nothing has changed,
	 * and the pointer is consistent with the allocation
	 * descriptor 
	 */
	assert(!already_used);
	if (__cbuf_alloc_meta_inconsistent(d, cm) || already_used || !mapped_in) {
		/* 
		 * This is complicated.
		 *
		 * See cbuf_slab_free for the rest of the story.  
		 *
		 * Assumptions: 
		 * 
		 * 1) The cbuf manager shared the cbuf meta (in the
		 * meta_cbuf vector) information with this component.
		 * It can remove asynchronously a cbuf from this
		 * structure at any time IFF that cbuf is marked as
		 * ~CBUF_IN_USE.
		 *
		 * 2) The slab descriptors, and the slab_desc vector
		 * are _not_ shared with the cbuf manager for
		 * complexity reasons.
		 *
		 * Question: How do we reconcile the fact that the
		 * cbuf mgr might remove at any point a cbuf from this
		 * component, but we still have a slab descriptor
		 * lying around for it?  How will we know that the
		 * cbuf has been removed, and not to use the slab
		 * data-structure anymore?
		 *
		 * Answer: The slabs are deallocated lazily (seen
		 * here).  When a slab is pulled off of the freelist
		 * (see the following code), we check to make sure
		 * that the cbuf meta information matches up with the
		 * slab's information (i.e. the cbuf id and the
		 * address in memory of the cbuf.  If they do not,
		 * then we know that the slab is outdated and that the
		 * cbuf backing it has been taken from this component.
		 * In that case (shown in the following code), we
		 * delete the slab descriptor.  Again, see
		 * cbuf_slab_free to see the surprising fact that we
		 * do _not_ deallocate the slab descriptor there to
		 * reinforce that point.
		 */
		__cbuf_desc_free(d);
		if (likely(!already_used)) {
			cm->nfo.c.flags &= ~(CBUFM_IN_USE | CBUFM_TOUCHED);
		}
		goto again;
	}

	cm->thdid_owner = cos_get_thd_id();
	cos_comp_info.cos_tmem_available[COMP_INFO_TMEM_CBUF]--;
	ret = (void*)(cm->nfo.c.ptr << PAGE_ORDER);
done:
	*cb = cbuf_cons(cbid, len);
	CBUF_RELEASE();

	return ret;
}

static inline void
cbuf_free(void *buf)
{
	u32_t idx = ((u32_t)buf) >> PAGE_ORDER;
	struct cbuf_alloc_desc *d, *fl;
	struct cbuf_meta *cm;

	CBUF_TAKE();
	d  = __cbuf_alloc_lookup(idx);
	assert(d);
	cm = cbuf_vect_lookup_addr(&meta_cbuf, cbid_to_meta_idx(d->cbid));
	assert(!__cbuf_alloc_meta_inconsistent(d, cm));
	assert(cm->nfo.c.flags & CBUFM_IN_USE);

	fl = d->flhead;
	assert(fl);
	ADD_LIST(fl, d, next, prev);
	/* do this last, so that we can guarantee the manager will not steal the cbuf before now... */
	cm->nfo.c.flags &= ~CBUFM_IN_USE;
	cm->thdid_owner = 0;
	cos_comp_info.cos_tmem_available[COMP_INFO_TMEM_CBUF]++;
	CBUF_RELEASE();

	/* Does the manager want the memory back? */
	if (unlikely(cos_comp_info.cos_tmem_relinquish[COMP_INFO_TMEM_CBUF])) {
		cbuf_c_delete(cos_spd_id(), d->cbid);
		assert(lock_contested(&cbuf_lock) != cos_get_thd_id());
		return;
	} 
}

/* Is it a cbuf?  If so, what's its id? */
static inline int 
cbuf_id(void *buf)
{
	u32_t idx = ((u32_t)buf) >> PAGE_ORDER;
	struct cbuf_alloc_desc *d;
	int id;

	CBUF_TAKE();
	d  = __cbuf_alloc_lookup(idx);
	id = (likely(d)) ? d->cbid : 0;
	CBUF_RELEASE();
	assert(lock_contested(&cbuf_lock) != cos_get_thd_id());

	return id;
}

/* Is the pointer a cbuf?  */
static inline int 
cbuf_is_cbuf(void *buf)
{
	return cbuf_id(buf) != 0;
}

#endif /* CBUF_H */

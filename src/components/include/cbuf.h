/**
 * Copyright 2010 by Gabriel Parmer.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gparmer@gwu.edu, 2010
 * Updated by Qi Wang and Jiguo Song, 2011
 * Updated and simplified by removing sub-page allocations, Gabe Parmer, 2012
 * Updated to add persistent cbufs, Gabe Parmer, 2012
 */

/* 
 * You'll require dependencies on both cbuf_c and cbufp.
 */

#ifndef  CBUF_H
#define  CBUF_H

#include <cos_component.h>
#include <cos_debug.h>
#include <cbuf_c.h>
#include <cbufp.h>
#define cbid_to_meta_idx(cid) ((cid) << 1)
#define meta_to_cbid_idx(mid) ((mid) >> 1)
#include <cbuf_vect.h>
#include <cos_list.h>
#include <bitmap.h>
#include <cos_synchronization.h>

#include <cos_alloc.h>
#define CVECT_ALLOC() alloc_page()
#define CVECT_FREE(x) free_page(x)
#include <cvect.h>

#include <tmem_conf.h>

extern cos_lock_t cbuf_lock;
#define CBUF_TAKE()    do { if (unlikely(lock_take_up(&cbuf_lock))) BUG(); } while(0)
#define CBUF_RELEASE() do { if (unlikely(lock_release_up(&cbuf_lock))) BUG(); } while(0)

#define CBUFP_MAX_NSZ 64

/* 
 * The lifetime of tmem cbufs is determined by the cbuf_alloc and
 * cbuf_free of the calling component.  The called component uses
 * cbuf2buf to retrieve the buffer.  
 *
 * API for sender of transient memory cbufs:
 *     void *cbuf_alloc(int size, cbuf_t *id)
 *     void cbuf_free(void *)
 * API for the receiver of transient memory cbufs:
 *     void *cbuf2buf(cbuf_t cb, int len)
 * 
 * Persistent cbufs are reference counted, so their lifetime needs to
 * be explicitly managed by calls to access and free the buffer, both
 * in the sender and receiver.  In the sender, alloc references it,
 * send tracks the liveness, and deref releases the buffer.
 * send_deref both sends and dereferences it.  The receiver of the
 * persistent cbuf references the cbuf via cbuf2buf.  The receiver
 * must use send an d deref just as the sender if it is also going to
 * send the buffer.
 *
 * API for sender of persistent cbufs:
 *     void *cbufp_alloc(unsigned int sz, cbufp_t *cb)
 *     void cbufp_send(cbufp_t cb)
 *     void cbufp_deref(cbufp_t cbid) 
 *     void cbufp_send_deref(cbufp_t cb) // combine the previous ops
 * API for the receiver of persistent cbufs;
 *     void *cbufp2buf(cbufp_t cb, int len)
 *     void cbufp_deref(cbufp_t cbid) 
 * Note that receiver might take the role of the sender.
 */

/* 
 * Naming scheme: The cbuf_* functions are transient memory
 * allocations -- their lifetime must match an invocation.  The
 * cbufp_* functions are persistent cbufs that can have any lifetime
 * (but that don't come with a guarantee about bounded access, thus
 * predictable sharing of the buffer).
 */

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

typedef u32_t  cbuf_t;   /* Requirement: gcc will return this in a register */
typedef cbuf_t cbufp_t;	/* ...to differentiate in interfaces with normal cbufs */
typedef union {
	cbuf_t v;
	struct {
		/* 
		 * cbuf id, aggregate = 1 if this cbuf holds an array
		 * of other cbufs.
		 */
		u32_t aggregate: 1, id:31;
	} __attribute__((packed)) c;
} cbuf_unpacked_t;

/* TODO: use these! */
struct cbuf_agg_elem {
	cbuf_t id;
	u32_t offset, len;
};
/* An aggregate of multiple cbufs. */
struct cbuf_agg {
	int ncbufs;
	struct cbuf_agg_elem elem[0];
};

static inline void 
cbuf_unpack(cbuf_t cb, u32_t *cbid) 
{
	cbuf_unpacked_t cu = {0};
	
	cu.v  = cb;
	*cbid = cu.c.id;
	assert(!cu.c.aggregate);
	return;
}

static inline cbuf_t 
cbuf_cons(u32_t cbid, u32_t len) 
{
	cbuf_unpacked_t cu;
	cu.v     = 0;
	cu.c.id  = cbid;
	return cu.v; 
}

static inline cbuf_t cbuf_null(void)      { return 0; }
static inline int cbuf_is_null(cbuf_t cb) { return cb == 0; }

extern struct cbuf_alloc_desc *__cbuf_alloc_slow(int size, int *len, int tmem);
extern int  __cbuf_2buf_miss(int cbid, int len, int tmem);
extern void __cbuf_desc_free(struct cbuf_alloc_desc *d);
extern cvect_t meta_cbuf, meta_cbufp;

static inline struct cbuf_meta *
cbuf_vect_lookup_addr(long idx, int tmem)
{
	if (tmem) return cvect_lookup_addr(&meta_cbuf,  idx);
	else      return cvect_lookup_addr(&meta_cbufp, idx);
}

/* 
 * Common case.  This is the most optimized path.  Every component
 * that wishes to access a cbuf created by another component must use
 * this function to map the cbuf_t to the actual buffer.
 */
static inline void * 
__cbuf2buf(cbuf_t cb, int len, int tmem)
{
	u32_t id;
	struct cbuf_meta *cm;
	union cbufm_info ci;//, ci_new;
	void *ret = NULL;
	long cbidx;
	if (unlikely(!len)) return NULL;
	cbuf_unpack(cb, &id);

	CBUF_TAKE();
	cbidx = cbid_to_meta_idx(id);
again:
	do {
		cm = cbuf_vect_lookup_addr(cbidx, tmem);
		if (unlikely(!cm || cm->nfo.v == 0)) {
			if (__cbuf_2buf_miss(id, len, tmem)) goto done;
			goto again;
		}
	} while (unlikely(!cm->nfo.v));
	ci.v = cm->nfo.v;

	if (!tmem) {
		if (unlikely(cm->nfo.c.flags & CBUFM_TMEM)) goto done;
		if (unlikely((len >> PAGE_ORDER) > cm->sz)) goto done;
		/*cm->nfo.c.flags |= CBUFM_IN_USE;*/
		if(cm->nfo.c.refcnt == CBUFP_REFCNT_MAX)
			assert(0);
		cm->nfo.c.refcnt++;
		assert(cm->owner_nfo.c.nrecvd < TMEM_SENDRECV_MAX);
		cm->owner_nfo.c.nrecvd++;
	} else {
		if (unlikely(!(cm->nfo.c.flags & CBUFM_TMEM))) goto done;
		if (unlikely(len > PAGE_SIZE)) goto done;
	}
	/* if (unlikely(cm->sz && (((int)cm->sz)<<PAGE_ORDER) < len)) goto done; */
	/* ci_new.v       = ci.v; */
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

static inline void *
cbuf2buf(cbuf_t cb, int len) { return __cbuf2buf(cb, len, 1); }
static inline void *
cbufp2buf(cbufp_t cb, int len) { return __cbuf2buf((cbuf_t)cb, len, 0); }

/* 
 * This is only called for permanent cbufs.  This is called every time
 * we wish to send the cbuf to another component.  For each cbufp2buf,
 * it should be called one or more times (i.e. for each component it
 * sends the cbuf to.  If the component is instead done with the cbuf,
 * and does not want to send it anywhere, it should cbufp_free it
 * instead.  So to be more precise:
 *
 * A component that cbuf2bufs a cbuf can cbufp_send it one or more
 * times to other components.  The last time we send the cbuf, free=1,
 * or the component can call cbufp_free.
 */
static inline void
__cbufp_send(cbuf_t cb, int free)
{
	u32_t id;
	struct cbuf_meta *cm;

	cbuf_unpack(cb, &id);

	CBUF_TAKE();
	cm = cbuf_vect_lookup_addr(cbid_to_meta_idx(id), 0);

	assert(cm && cm->nfo.v);
	/* assert(cm->nfo.c.flags & CBUFM_IN_USE); */
	assert(cm->nfo.c.refcnt);
	assert(!(cm->nfo.c.flags & CBUFM_TMEM));
	assert(cm->owner_nfo.c.nsent < TMEM_SENDRECV_MAX);

	cm->owner_nfo.c.nsent++;
	/* if (free) cm->nfo.c.flags &= ~CBUFM_IN_USE; */
	if (free) cm->nfo.c.refcnt--;
	/* really should deal with this case correctly */
	assert(!(cm->nfo.c.flags & CBUFM_RELINQ)); 
	CBUF_RELEASE();
}

static inline void
cbufp_send_deref(cbufp_t cb)
{ __cbufp_send(cb, 1); }

static inline void
cbufp_send(cbufp_t cb) 
{ __cbufp_send(cb, 0); }

extern cvect_t alloc_descs; 
struct cbuf_alloc_desc {
	int cbid, length, tmem;
	void *addr;
	struct cbuf_meta *meta;
	struct cbuf_alloc_desc *next, *prev, *flhead; /* freelist */
};
extern struct cbuf_alloc_desc cbuf_alloc_freelists;
extern struct cbuf_alloc_desc cbufp_alloc_freelists[];

static inline struct cbuf_alloc_desc *
__cbuf_alloc_lookup(int page_index) { return cvect_lookup(&alloc_descs, page_index); }

/* 
 * Assume that m was retrieved with 
 * m = cbuf_vect_lookup_addr(cbid_to_meta_idx(d->cbid), tmem);
 * This validates that the d->cbid = cbid of cbuf_meta.
 */
static inline int
__cbuf_alloc_meta_inconsistent(struct cbuf_alloc_desc *d, struct cbuf_meta *m)
{
	assert(d && m && d->addr);
	/* we don't want the manager changing this under us */
	/* assert(m->nfo.c.flags & CBUFM_IN_USE); */
	assert(m->nfo.c.refcnt);
	return (unlikely((unsigned long)d->addr >> PAGE_SHIFT != m->nfo.c.ptr ||
			 d->meta != m /*|| length*/));
}

/* 
 * Simple power-of-two allocator.  Later we can investigate going to
 * something with more precision, or a slab.  We are currently leaving
 * CBUFP_MAX_NSZ - (WORD_SIZE - PAGE_ORDER) = 64 - (32-12) = 44 unused
 * sizes that could be expanded for both of these uses.
 *
 * TODO: it appears that the compiler is not able to statically
 * calculate this...  This is a big problem for allocations of a fixed
 * size where this should translate into a direct freelist access with
 * no calculations.  Verify this and fix.
 *
 * precondition:  size must be a power of 2 && >= PAGE_SIZE
 */
static inline struct cbuf_alloc_desc *
__cbufp_freelist_get(int size)
{
	int order = ones(size-1) - PAGE_ORDER;
	struct cbuf_alloc_desc *d;

	assert(pow2(size) && size >= PAGE_SIZE);
	assert(order >= 0 && order < WORD_SIZE-PAGE_ORDER);
	d = &cbufp_alloc_freelists[order];
	assert(d->length == size);

	return d;
}

static inline void *
__cbuf_alloc(unsigned int sz, cbuf_t *cb, int tmem)
{
	void *ret;
	struct cbuf_alloc_desc *d, *fl;
	int cbid, len, already_used, mapped_in, flags;
	struct cbuf_meta *cm;
	long cbidx;

	if (tmem) {
		fl = &cbuf_alloc_freelists;
	} else {
		/* need a size >= PAGE_ORDER, that is a power of 2 */
		sz = nlepow2(round_up_to_page(sz));
		fl = __cbufp_freelist_get(sz);
	}
	CBUF_TAKE();
again:
	d = FIRST_LIST(fl, next, prev);
	if (unlikely(EMPTY_LIST(d, next, prev))) {
		d    = __cbuf_alloc_slow(sz, &len, tmem);
		assert(d);
		ret  = d->addr;
		cbid = d->cbid;
		goto done;
		/*
		 * TODO: check if this cbuf has been taken by another
		 * thd already.  shall we add this cbuf to the
		 * freelist and just continue?
		 */
	} 
	REM_LIST(d, next, prev);
	assert(EMPTY_LIST(d, next, prev));
	cbid  = d->cbid;
	assert(cbid);
	cbidx            = cbid_to_meta_idx(cbid);
	cm               = cbuf_vect_lookup_addr(cbidx, tmem);

	mapped_in        = cbufm_is_mapped(cm);
	/* already_used     = cm->nfo.c.flags & CBUFM_IN_USE; */
	/* flags            = CBUFM_IN_USE | CBUFM_TOUCHED; */ /* should be atomic */
	already_used     = cm->nfo.c.refcnt;
	flags            = CBUFM_TOUCHED;
	if (tmem) flags |= CBUFM_TMEM;
	cm->nfo.c.flags |= flags;
	if(cm->nfo.c.refcnt == CBUFP_REFCNT_MAX)
		assert(0);
	cm->nfo.c.refcnt++;

	/* 
	 * Now that IN_USE is set, we know the manager will not rip
	 * this out from under us.  Check that nothing has changed,
	 * and the pointer is consistent with the allocation
	 * descriptor 
	 */
	assert(!already_used);
	if (unlikely(__cbuf_alloc_meta_inconsistent(d, cm) || already_used || !mapped_in)) {
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
			/* cm->nfo.c.flags &= ~(CBUFM_IN_USE | CBUFM_TOUCHED); */
			cm->nfo.c.flags &= ~CBUFM_TOUCHED;
			cm->nfo.c.refcnt--;
		}
		goto again;
	}

	if (tmem) {
		cm->owner_nfo.thdid = cos_get_thd_id();
		cos_comp_info.cos_tmem_available[COMP_INFO_TMEM_CBUF]--;
		assert(cm->nfo.c.flags & CBUFM_TMEM);
	} else {
		cm->owner_nfo.c.nsent = cm->owner_nfo.c.nrecvd = 0;		
	}
	ret = (void*)(cm->nfo.c.ptr << PAGE_ORDER);
done:
	*cb = cbuf_cons(cbid, len);
	CBUF_RELEASE();
	return ret;
}

static inline void *
cbuf_alloc(unsigned int sz, cbuf_t *cb) { return __cbuf_alloc(sz, cb, 1); }
static inline void *
cbufp_alloc(unsigned int sz, cbufp_t *cb)  { return __cbuf_alloc(sz, (cbuf_t*)cb, 0); }

/* 
 * precondition:  cbuf lock must be taken.
 * postcondition: cbuf lock has been released.
 */
static inline void
__cbuf_done(int cbid, int tmem, struct cbuf_alloc_desc *d)
{
	struct cbuf_meta *cm;
	int owner, relinq = 0;

	cm = cbuf_vect_lookup_addr(cbid_to_meta_idx(cbid), tmem);
	assert(!d || !__cbuf_alloc_meta_inconsistent(d, cm));
	/* 
	 * If this assertion triggers, one possibility is that you did
	 * not successfully map it in (cbufp2buf or cbufp_alloc).
	 */
	/* assert(cm->nfo.c.flags & CBUFM_IN_USE); */
	assert(cm->nfo.c.refcnt);
	owner = cm->nfo.c.flags & CBUFM_OWNER;
	assert(!(tmem & !owner)); /* Shouldn't be calling free... */

	if (tmem && d && owner) {
		struct cbuf_alloc_desc *fl;

		fl = d->flhead;
		assert(fl);
		ADD_LIST(fl, d, next, prev);
		cm->owner_nfo.thdid = 0;
	}
	/* do this last, so that we can guarantee the manager will not steal the cbuf before now... */
	/* cm->nfo.c.flags &= ~CBUFM_IN_USE; */
	cm->nfo.c.refcnt--;
	if (tmem) cos_comp_info.cos_tmem_available[COMP_INFO_TMEM_CBUF]++;
	else      relinq = cm->nfo.c.flags & CBUFM_RELINQ;
	CBUF_RELEASE();
	
	/* Does the manager want the memory back? */
	if (tmem) {
		if (unlikely(cos_comp_info.cos_tmem_relinquish[COMP_INFO_TMEM_CBUF])) {
			cbuf_c_delete(cos_spd_id(), cbid);
			assert(lock_contested(&cbuf_lock) != cos_get_thd_id());
			return;
		}
	} else if (unlikely(relinq)) {
		cbufp_delete(cos_spd_id(), cbid);
		assert(lock_contested(&cbuf_lock) != cos_get_thd_id());
		return;
	}
	
	return;
}

static inline void
cbufp_deref(cbufp_t cbid) 
{ 
	u32_t id;
	
	cbuf_unpack(cbid, &id);
	CBUF_TAKE();
	__cbuf_done((int)id, 0, NULL);
}

static inline void
__cbuf_free(void *buf, int tmem)
{
	u32_t idx = ((u32_t)buf) >> PAGE_ORDER;
	struct cbuf_alloc_desc *d;
	int cbid;

	CBUF_TAKE();
	d  = __cbuf_alloc_lookup(idx);
	assert(d);
	if (unlikely(d->tmem != tmem)) goto err;
	cbid = d->cbid;
	/* note: lock released in function */
	__cbuf_done(cbid, tmem, d);

	return;
err:
	CBUF_RELEASE();
	return;
}

static inline void
cbuf_free(void *buf) { __cbuf_free(buf, 1); }

/* 
 * Is it a cbuf?  If so, what's its id? 
 *
 * This only works in the allocating component (with the freelist
 * descriptor).
 */
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

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
#include <util.h>
#include <cos_debug.h>
#include <cbuf_meta.h>
#include <cbuf_mgr.h>
#define cbid_to_meta_idx(cid) ((cid) << 2)
#define meta_to_cbid_idx(mid) ((mid) >> 2)
#include <cbuf_vect.h>
#include <cos_list.h>
#include <bitmap.h>
#include <cos_synchronization.h>

#include <cos_alloc.h>
#define CVECT_ALLOC() alloc_page()
#define CVECT_FREE(x) free_page(x)
#include <cvect.h>

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

typedef union {
	cbuf_t v;
	struct {
		/* 
		 * cbuf id, aggregate = 1 if this cbuf holds an array of other
		 * cbufs, tm = 1 if this cbuf is transient (non-persistent)
		 */
		u32_t aggregate: 1, tm: 1, id:30;
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
cbuf_cons(u32_t cbid)
{
	cbuf_unpacked_t cu;
	cu.v     = 0;
	cu.c.id  = cbid;
	return cu.v; 
}

static inline cbuf_t cbuf_null(void)      { return 0; }
static inline int cbuf_is_null(cbuf_t cb) { return cb == 0; }

extern struct cbuf_meta * __cbuf_alloc_slow(int size, int *len);
extern int  __cbuf_2buf_miss(int cbid, int len);
extern cvect_t meta_cbuf;

static inline struct cbuf_meta *
cbuf_vect_lookup_addr(long idx)
{
	return cvect_lookup_addr(&meta_cbuf,  idx);
}

/* 
 * Common case.  This is the most optimized path.  Every component
 * that wishes to access a cbuf created by another component must use
 * this function to map the cbuf_t to the actual buffer. This function
 * returns an error (NULL) if called by the owner of the cbuf_t.
 */
static inline void * 
cbuf2buf(cbuf_t cb, int len)
{
	u32_t id;
	struct cbuf_meta *cm;
	void *ret = NULL;
	long cbidx;
	int t;

	if (unlikely(!len)) return NULL;
	cbuf_unpack(cb, &id);
	cbidx = cbid_to_meta_idx(id);
again:
	do {
		cm = cbuf_vect_lookup_addr(cbidx);
		if (unlikely(!cm || cm->nfo == 0)) {
			if (__cbuf_2buf_miss(id, len)) goto done;
			goto again;
		}
	} while (unlikely(!cm->nfo));
	/* shouldn't cbuf2buf your own buffer! */
	assert(cm->cbid.cbid == id);
	if (unlikely(CBUF_OWNER(cm))) goto done;
	if (unlikely((len >> PAGE_ORDER) > cm->sz)) goto done;
	assert(CBUFM_GET_REFCNT(cm) < CBUF_REFCNT_MAX);
	CBUFM_INC_REFCNT(cm);
	assert(cm->owner_nfo.c.nrecvd < CBUF_SENDRECV_MAX);
	cm->owner_nfo.c.nrecvd++;      //???
	ret = ((void*)(CBUFM_GET_PTR(cm) << PAGE_ORDER));
done:	
	return ret;
}

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
__cbuf_send(cbuf_t cb, int free)
{
	u32_t id;
	struct cbuf_meta *cm;

	cbuf_unpack(cb, &id);

	cm = cbuf_vect_lookup_addr(cbid_to_meta_idx(id));
	assert(cm && cm->nfo);
	assert(CBUFM_GET_REFCNT(cm));
	assert(cm->owner_nfo.c.nsent < CBUF_SENDRECV_MAX);

	cm->owner_nfo.c.nsent++;
	if (free) CBUFM_DEC_REFCNT(cm);
	/* really should deal with this case correctly */
	assert(!CBUF_RELINQ(cm));     //???
}

static inline void
cbuf_send_deref(cbuf_t cb)
{ __cbuf_send(cb, 1); }

static inline void
cbuf_send(cbuf_t cb) 
{ __cbuf_send(cb, 0); }

struct cbuf_freelist {
	struct cbuf_meta freelist_head[64];    //???
};
PERCPU_DECL(struct cbuf_freelist, cbuf_alloc_freelists);
PERCPU_EXTERN(cbuf_alloc_freelists);
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
static inline struct cbuf_meta *
__cbuf_freelist_get(int size)
{
	struct cbuf_freelist *fl = PERCPU_GET(cbuf_alloc_freelists);
	int order = ones(size-1) - PAGE_ORDER;
	struct cbuf_meta *m;
	assert(pow2(size) && size >= PAGE_SIZE);
	assert(order >= 0 && order < WORD_SIZE-PAGE_ORDER);
	m = &(fl->freelist_head[order]);
	if (!CBUFM_GET_NEXT(m)) {
		CBUFM_SET_NEXT(m, m);
	}
	return m;
}

static inline void *
cbuf_alloc(unsigned int sz, cbuf_t *cb, int tmem)
{
	void *ret;
	int cbid, len, already_used, inconsistent, mapped_in, flags;
	struct cbuf_meta *cm, *fl, old_head, new_head;
	unsigned long long *target, *old, *update;
	long cbidx;
	sz = nlepow2(round_up_to_page(sz));
	fl = __cbuf_freelist_get(sz);
again:
	target = (unsigned long long *)(&fl->next_flag);
	old    = (unsigned long long *)(&old_head.next_flag);
	update = (unsigned long long *)(&new_head.next_flag);
	do {
		*old   = *target;
		cm		   = CBUFM_GET_NEXT(fl);
		new_head.next_flag = cm->next_flag;
		new_head.cbid.tag  = fl->cbid.tag+1;
	} while(unlikely(!cos_dcas(target, *old, *update)));
	if (unlikely(cm == CBUFM_GET_NEXT(cm))) {
		cm   = __cbuf_alloc_slow(sz, &len);
		assert(cm);
		goto done;
		/*
		 * TODO: check if this cbuf has been taken by another
		 * thd already.  shall we add this cbuf to the
		 * freelist and just continue?
		 */   //???
	}
	already_used = CBUFM_GET_REFCNT(cm);
	assert(!already_used);
	inconsistent = CBUF_INCONSISENT(cm);
	if (unlikely(inconsistent)) {
		CBUFM_SET_NEXT_NULL(cm);
		goto again;
	}
	unsigned int p, n;
	p = cm->nfo;
	n = p+CBUFM_INC_UNIT;
	if (unlikely(!cos_cas((unsigned long *)&cm->nfo, p, n))) {
		CBUFM_SET_NEXT_NULL(cm);
		goto again;
	}
	CBUFM_SET_NEXT_NULL(cm);
	cm->owner_nfo.c.nsent = cm->owner_nfo.c.nrecvd = 0;
	/*if (unlikely(tmem)) {
		cm->owner_nfo.thdid = cos_get_thd_id();
		cos_comp_info.cos_tmem_available[COMP_INFO_TMEM_CBUF]--;
		assert(cm->nfo.c.flags & CBUFM_TMEM);
	} */     //???
done:
	if (unlikely(tmem)) CBUF_SET_TMEM(cm);
	ret = (void *)(CBUFM_GET_PTR(cm) << PAGE_ORDER);
	cbid = cm->cbid.cbid;
	*cb = cbuf_cons(cbid);
	return ret;
}

/* 
 * precondition:  cbuf lock must be taken.
 * postcondition: cbuf lock has been released.
 */
static inline void
cbuf_free(cbuf_t cb)
{
	u32_t id;
	cbuf_unpack(cb, &id);
	struct cbuf_meta *cm, *fl, head, new_head;
	int owner, relinq = 0, tmem, inconsistent;
	unsigned long long *target, *old, *update;

	cm = cbuf_vect_lookup_addr(cbid_to_meta_idx(id));
	/* 
	 * If this assertion triggers, one possibility is that you did
	 * not successfully map it in (cbufp2buf or cbufp_alloc).
	 */
	assert(CBUFM_GET_REFCNT(cm));
	owner = CBUF_OWNER(cm);
	tmem = CBUF_TMEM(cm);
	inconsistent = CBUF_INCONSISENT(cm);
	assert(!inconsistent);
	if (tmem) {
		assert(owner);
		fl = __cbuf_freelist_get(cm->sz << PAGE_ORDER);
		CBUFM_SET_NEXT(cm, CBUFM_GET_NEXT(fl));
		cos_mem_fence();
		CBUFM_DEC_REFCNT(cm);
		cos_mem_fence();
		target = (unsigned long long *)(&fl->next_flag);
		old    = (unsigned long long *)(&head.next_flag);
		update = (unsigned long long *)(&new_head.next_flag);
		new_head.next_flag = cm;
		do {
			*old = *target;
			new_head.cbid.tag  = fl->cbid.tag+1;
		} while(unlikely(!cos_dcas(target, *old, *update)));
	}
	else CBUFM_DEC_REFCNT(cm);      
	relinq = CBUF_RELINQ(cm);
	/* Does the manager want the memory back? */
	if (unlikely(relinq)) cbuf_delete(cos_spd_id(), id);	
	return;
}

/* Is the pointer a cbuf?  */
static inline int 
cbuf_is_cbuf(void *buf)
{
	return false;   //???
}

#endif /* CBUF_H */

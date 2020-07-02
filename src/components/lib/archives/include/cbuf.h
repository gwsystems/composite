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

#ifndef CBUF_H
#define CBUF_H

#include <cos_component.h>
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

/*
 * Cbufs are reference counted, so their lifetime needs to be
 * explicitly managed by calls to access and free the buffer, both
 * in the sender and receiver.  In the sender, alloc references it,
 * send tracks the liveness, and free releases the buffer. send_free
 * both sends and dereferences it.  The receiver of the cbuf
 * references the cbuf via cbuf2buf.  The receiver must use send and
 * free just as the sender if it is also going to send the buffer.
 *
 * API for sender of cbufs:
 *     void *cbuf_alloc(unsigned int sz, cbuf_t *cb)
 *     void cbuf_send(cbuf_t cb)
 *     void cbuf_free(cbuf_t cbid)
 *     void cbuf_send_free(cbuf_t cb) // combine the previous ops
 * API for the receiver of persistent cbufs;
 *     void *cbuf2buf(cbuf_t cb, int len)
 *     void cbuf_free(cbuf_t cbid)
 * Note that receiver might take the role of the sender.
 * Extended api for cbuf_alloc
 *     void *cbuf_alloc_ext(unsigned long sz, cbuf_t *cb, unsigned int flag)
 * currently it supports two extended flags:
 * CBUF_TMEM: Those cbufs will be managed by cbuf's owner instead of
 * cbuf manager. After owner free a cbuf, it can reuse it immediately.
 * CBUF_EXACTSZ: The size of thus cbuf will not be round to power-of-2.
 * But it is still multiple times of PAGE_SIZE. Those cbufs will be
 * immediately returned to cbuf manager when it is freed.
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
 * component, b) some flag bits, c) reference counter and two additional
 * counters for send/receive, d) the size of objects held in this page.
 * e) "next" pointer of the embedded freelist, and f) its cbuf id.
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
 * 1) At client side, each thread use atomic instructions to
 * manipulate the meta data, and most of them are wait-free.
 * The exception is the implementation of lock-free freelist.
 * It needs a double-cas loop for both add and remove operation.
 *
 * 2) However at manager side, it still a big lock to protect
 * the whole component. Every interface of the manager has to
 * require the lock first, and release it when return.
 *
 * 3) While the reference counter is not zero in a meta entry,
 * the cbuf manager will not remove the cbuf from us and from
 * the meta-entry. Therefore, increasing the reference counter
 * acts as a form of a lock between this component and the manager.
 */

/***
 * On data-structure consistency:
 *
 * The cbuf_meta data-structure is shared between manager and client,
 * and is one-to-one mapped with the cbufs themselves.  These are
 * indexed by the cbuf id.
 *
 * When the reference counter is zero in the cbuf_meta structure and
 * all potential receivers have received this cbuf (i.e. it is
 * in the common case on a free-list in the client), the manager can
 * remove it at any time. This means that free-list entries can exist
 * that don't actually refer to a cbuf. We call these stale free-list
 * entries that must be cleaned up.
 *
 * The manager never collect cbufs which are on free-list. Such cbufs
 * are reachable from client, so they are not garbage. Thus a
 * non-null next pointer can prevent a cbuf from being collected.
 * Whenever the manager removes a cbuf, it will check if it is on a
 * free-list (check the "next" pointer in meta data). If it is, the
 * manager will set the inconsistent bit in the meta data.
 *
 * When a thread gets a cbuf from its free-list, it must check the
 * inconsistent bit first. It this is a valid entry then increase the
 * refcnt with CAS to detect concurrent modification from the manager.
 * If CAS fails, it just discards this cbuf and tries to take next
 * one. If this is not a valid entry, the client needs to clear the
 * inconsistent bit and gets next one.
 *
 * Some edge cases:
 * 1) Before the stale entry is dropped, another thread on different
 * core may asks the manager for new cbuf. The manager should avoid to
 * allocate the same cbuf id as the invalid entry. To achieve this,
 * in cbuf_create the manager always checks if there is invalid meta
 * data associated the id in the calling component.
 *
 * 2) a) the manager removes a cbuf with id K from the component c0 (but
 * the free-list entry still remains), b) another component c1 creates
 * new cbuf. The manager also assigns id K to the new cbuf. c) c1 sends
 * this cbuf back to the c0. Does it matter for c0 to receive this cbuf?
 * No. In cbuf2buf, there is no need to touch free-list and to check
 * inconsistent bit. So it can receive this cbuf as normal with those
 * flags intact. Later on, when c0 alloc cbuf, it can find the invalid
 * entry and skip it. This also implies when discard invalid entry, we
 * can only unset the inconsistent bit without modifying other fields.
 */

typedef u32_t cbuf_t; /* fit in a register. */
typedef union {
	cbuf_t v;
	struct {
		/*
		 * cbuf id, aggregate = 1 if this cbuf
		 * holds an array of other cbufs
		 */
		u32_t id : 31, aggregate : 1;
	} __attribute__((packed)) c;
} cbuf_unpacked_t;

/* TODO: use these! */
struct cbuf_agg_elem {
	cbuf_t id;
	u32_t  offset, len;
};
/* An aggregate of multiple cbufs. */
struct cbuf_agg {
	int                  ncbufs;
	struct cbuf_agg_elem elem[0];
};

static inline void
cbuf_unpack(cbuf_t cb, unsigned int *cbid)
{
	cbuf_unpacked_t cu = {0};
	cu.v               = cb;
	*cbid              = cu.c.id;
	assert(!cu.c.aggregate);
	return;
}

static inline cbuf_t
cbuf_cons(unsigned int cbid)
{
	cbuf_unpacked_t cu;
	cu.v    = 0;
	cu.c.id = cbid;
	return cu.v;
}

static inline cbuf_t
cbuf_null(void)
{
	return 0;
}
static inline int
cbuf_is_null(cbuf_t cb)
{
	return cb == 0;
}

extern struct cbuf_meta *__cbuf_alloc_slow(unsigned long size, int *len, unsigned int flag);
extern int               __cbuf_2buf_miss(unsigned int cbid, int len);
extern cvect_t           meta_cbuf;
static inline void       cbuf_free(cbuf_t cb);

static inline struct cbuf_meta *
cbuf_vect_lookup_addr(unsigned int id)
{
	return (struct cbuf_meta *)cvect_lookup_addr(&meta_cbuf, cbid_to_meta_idx(id));
}

/*
 * Common case.  This is the most optimized path.  Every component
 * that wishes to access a cbuf created by another component must use
 * this function to map the cbuf_t to the actual buffer. This function
 * returns an error (NULL) if called by the owner of the cbuf_t or this
 * cbuf is not exist.
 */
static inline void *
cbuf2buf(cbuf_t cb, int len)
{
	struct cbuf_meta *cm;
	void *            ret = NULL;
	unsigned int      id, t;

	if (unlikely(len <= 0)) return NULL;
	cbuf_unpack(cb, &id);
again:
	cm = cbuf_vect_lookup_addr(id);
	if (unlikely(!cm || !CBUF_PTR(cm))) {
		if (__cbuf_2buf_miss(id, len)) goto done;
		goto again;
	}
	assert(cm->nfo);
	/* shouldn't cbuf2buf your own buffer! */
	assert(cm->cbid_tag.cbid == id);
	if (unlikely(CBUF_OWNER(cm))) goto done;
	if (unlikely((len >> PAGE_ORDER) > cm->sz)) goto done;
	assert(!CBUF_TMEM(cm));

	/*
	 * The order here is important. But as x86 does reorder stores, we only
	 * need compiler barrier. If the nrecv counter is increased first, the
	 * manager will find nsent = nreced, and both components' refcnt is 0.
	 * Thus it may remove this cbuf. In addition atomic instruction is
	 * also needed, because multiple threads can concurrently execute this code
	 */
	CBUF_REFCNT_ATOMIC_INC(cm);
	cos_compiler_barrier();
	CBUF_NRCV_ATOMIC_INC(cm);

	/* TODO: Issue #120. As we use faa to increase the counter, check overflow
	 * before increment is useless, but after increment the overflow is happen. */
	assert(CBUF_REFCNT(cm) > (unsigned int)(CBUF_REFCNT(cm) - 1));
	assert(cm->snd_rcv.nrecvd > (unsigned int)(cm->snd_rcv.nrecvd - 1));
	ret = (void *)CBUF_PTR(cm);
	assert(ret);
done:
	return ret;
}

/*
 * This is called every time we wish to send the cbuf to another
 * component. For each cbuf2buf, it should be called one or more
 * times (i.e. for each component it sends the cbuf to. If the
 * component is instead done with the cbuf, and does not want to
 * send it anywhere, it should cbuf_free it instead.
 * So to be more precise:
 *
 * A component that cbuf2bufs a cbuf can cbuf_send it one or more
 * times to other components.  The last time we send the cbuf, free=1,
 * or the component can call cbuf_free.
 */
static inline void
__cbuf_send(cbuf_t cb, int free)
{
	unsigned int      id;
	struct cbuf_meta *cm;

	cbuf_unpack(cb, &id);

	cm = cbuf_vect_lookup_addr(id);
	assert(cm && cm->nfo);
	assert(CBUF_REFCNT(cm));

	int old = cm->snd_rcv.nsent;
	CBUF_NSND_ATOMIC_INC(cm);
	if (!(cm->snd_rcv.nsent > old)) {
		printc("spd %d thd %d cb %d sent %d old %d\n", cos_spd_id(), cos_get_thd_id(), cb, cm->snd_rcv.nsent,
		       old);
	}
	assert(cm->snd_rcv.nsent > old);
	if (free) cbuf_free(cb);
}

static inline void
cbuf_send_free(cbuf_t cb)
{
	__cbuf_send(cb, 1);
}

static inline void
cbuf_send(cbuf_t cb)
{
	__cbuf_send(cb, 0);
}

struct cbuf_freelist {
	struct cbuf_meta freelist_head[CBUF_MAX_NSZ];
};
PERCPU_DECL(struct cbuf_freelist, cbuf_alloc_freelists);
PERCPU_EXTERN(cbuf_alloc_freelists);
/*
 * Simple power-of-two allocator.  Later we can investigate going to
 * something with more precision, or a slab.  We are currently leaving
 * CBUF_MAX_NSZ - (WORD_SIZE - PAGE_ORDER) = 64 - (32-12) = 44 unused
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
	/* We have per-core, per-component, per-size free-list */
	struct cbuf_freelist *fl    = PERCPU_GET(cbuf_alloc_freelists);
	int                   order = ones(size - 1) - PAGE_ORDER;
	struct cbuf_meta *    m;
	if (!(pow2(size) && size >= PAGE_SIZE)) {
		printc("cbuf free size %d spd %d\n", size, cos_spd_id());
	}
	assert(pow2(size) && size >= PAGE_SIZE);
	assert(order >= 0 && order < WORD_SIZE - PAGE_ORDER);
	m = &(fl->freelist_head[order]);
	assert(m->next);
	return m;
}

/*
 * Precondition: "next" pointer is not NULL.
 * We can reuse a cbuf from two source.
 * 1. from local freelist
 * 2. from garbage collection
 * The manager guarantees the precondition for case 2.
 *
 * Before next pointer is set to NULL, manager will not
 * collect it. See cbuf_collect for details.
 * As other clients or threads have no reference to this
 * cbuf, they will not access the meta concurrently.
 * But the manager may modify the meta at the same time.
 * So those atomic instructions are necessary
 */
static inline int
__cbuf_try_take(struct cbuf_meta *cm, unsigned int flag)
{
	int           inconsistent, r = 0;
	unsigned long old_nfo, new_nfo;

	assert(cm && cm->next);
	old_nfo = cm->nfo;
	assert(!(old_nfo & CBUF_REFCNT_MAX));

	inconsistent = old_nfo & CBUF_INCONSISTENT;
	if (unlikely(inconsistent)) {
		/*
		 * It has been or is going to be taken away by the
		 * manager. We will not leak cbuf here.
		 * Do not modify other fields!
		 */
		CBUF_FLAG_REM(cm, CBUF_INCONSISTENT);
		goto ret;
	}

	new_nfo = old_nfo + 1;
	if (unlikely(flag & CBUF_TMEM))
		new_nfo |= CBUF_TMEM;
	else
		new_nfo &= ~CBUF_TMEM;
	if (unlikely(!cos_cas(&cm->nfo, old_nfo, new_nfo))) {
		/*
		 * This failure maybe because:
		 * 1. manager tries to take this cbuf away, set inconsistent
		 * 2. manager set/unset relinq bit.
		 * For case 2, this cbuf can be collected later.
		 * No cbuf leak
		 */
		if (CBUF_INCONSISTENT(cm)) CBUF_FLAG_REM(cm, CBUF_INCONSISTENT);
		goto ret;
	}
	r = 1;
/*
 * guarantee the order between CAS and set "next".
 * If the next is set to NULL before increment refcnt, the
 * manager may think this cbuf is garbage and collect it.
 * But there is dependency between those two, we do not
 * need explicit barrier.
 */
ret:
	cm->next = NULL;
	return r;
}

static inline void
__cbuf_freelist_push(unsigned long sz, struct cbuf_meta *h, struct cbuf_meta *t)
{
	struct cbuf_meta *  fl;
	unsigned long long *target, old, update;
	unsigned long *     ofake = (unsigned long *)&old;
	unsigned long *     nfake = (unsigned long *)&update;

	fl       = __cbuf_freelist_get(sz);
	target   = (unsigned long long *)(&fl->next);
	nfake[0] = (unsigned long)h;
	do {
		old      = *(volatile unsigned long long *)target;
		t->next  = (struct cbuf_meta *)ofake[0];
		nfake[1] = ofake[1] + 1;
	} while (unlikely(!cos_dcas(target, old, update)));
}

static inline struct cbuf_meta *
__cbuf_freelist_pop(unsigned long sz, unsigned int flag)
{
	struct cbuf_meta *  cm, *fl;
	unsigned long long *target, old, update;
	unsigned long *     ofake, *nfake;

	ofake = (unsigned long *)&old;
	nfake = (unsigned long *)&update;
	fl    = __cbuf_freelist_get(sz);
again:
	/*
	 * This is a lock-free stack implementation. Add a
	 * tag to solve "ABA" problem.
	 */
	target = (unsigned long long *)(&fl->next);
	do {
		old      = *(volatile unsigned long long *)target;
		cm       = (struct cbuf_meta *)ofake[0];
		nfake[0] = (unsigned long)cm->next;
		nfake[1] = ofake[1] + 1;
	} while (unlikely(!cos_dcas(target, old, update)));

	if (unlikely(cm == fl)) return NULL;
	if (unlikely(!__cbuf_try_take(cm, flag))) goto again;
	return cm;
}

static inline void *
cbuf_set_fork(cbuf_t cb, int flag)
{
	unsigned int      id;
	struct cbuf_meta *cm;

	cbuf_unpack(cb, &id);

	cm = cbuf_vect_lookup_addr(id);
	assert(cm);
	cm->cbid_tag.tag = flag;
	return;
}

static inline void *
cbuf_alloc_ext(unsigned long sz, cbuf_t *cb, unsigned int flag)
{
	void *            ret;
	unsigned int      cbid;
	int               len;
	struct cbuf_meta *cm = NULL;

	if (unlikely(flag & CBUF_EXACTSZ)) goto create;

	sz = nlepow2(round_up_to_page(sz));
	cm = __cbuf_freelist_pop(sz, flag);
	if (unlikely(!cm)) goto create;

done:
	ret  = (void *)(CBUF_PTR(cm));
	cbid = cm->cbid_tag.cbid;
	*cb  = cbuf_cons(cbid);
	return ret;
create:
	/* TODO: Why use len? */
	cm = __cbuf_alloc_slow(sz, &len, flag);
	assert(cm);
	goto done;
}

static inline void *
cbuf_alloc(unsigned long sz, cbuf_t *cb)
{
	return cbuf_alloc_ext(sz, cb, 0);
}

static inline void
cbuf_ref(cbuf_t cb)
{
	struct cbuf_meta *cm;
	unsigned int      id;

	cbuf_unpack(cb, &id);
	cm = cbuf_vect_lookup_addr(id);
	assert(cm && CBUF_REFCNT(cm));
	CBUF_REFCNT_ATOMIC_INC(cm);
}

static inline void
cbuf_free(cbuf_t cb)
{
	struct cbuf_meta *cm, *fl;
	int               flag = CBUF_RELINQ | CBUF_TMEM | CBUF_EXACTSZ;
	unsigned int      id;

	cbuf_unpack(cb, &id);
	cm = cbuf_vect_lookup_addr(id);
	/*
	 * If this assertion triggers, one possibility is that you did
	 * not successfully map the cbuf in (cbuf2buf or cbuf_alloc).
	 */
	assert(cm && CBUF_REFCNT(cm));

	if (unlikely(cm->nfo & flag)) {
		/* Does the manager want the memory back? */
		/* Always delete exact size cbuf */
		if (CBUF_RELINQ(cm) || CBUF_EXACTSZ(cm)) {
			cbuf_delete(cos_spd_id(), id);
			return;
		}

		if (CBUF_TMEM(cm)) {
			assert(CBUF_OWNER(cm));
			/*
			 * non-null next pointer can prevent the manager
			 * to collect this cbuf. Again, we have to
			 * guarantee such order. If we first decrease
			 * the refcnt, the manager may collect this
			 * and give it to another thread who will put
			 * it in the free-list. Thus we may put the
			 * same cbuf on the free-list twice.
			 */
			cm->next = (struct cbuf_meta *)1;
			cos_compiler_barrier();
			CBUF_REFCNT_ATOMIC_DEC(cm);
			__cbuf_freelist_push(cm->sz << PAGE_ORDER, cm, cm);
		}
	} else {
		CBUF_REFCNT_ATOMIC_DEC(cm);
	}
	return;
}

/* Is the pointer a cbuf?  */
static inline int
cbuf_is_cbuf(void *buf)
{
	/* TODO: After using cbuf to back all memory, this is true */
	return false;
}


/*===== Debug functions ======*/
static int
cbuf_debug_freelist_num(unsigned long sz)
{
	struct cbuf_meta *cm, *fl;
	int               ret = 0;

	sz = nlepow2(round_up_to_page(sz));
	fl = __cbuf_freelist_get(sz);
	cm = fl->next;
	while (cm != fl) {
		ret++;
		cm = cm->next;
	}
	return ret;
}

static void
cbuf_debug_clear_freelist(unsigned long sz)
{
	cbuf_t cb;

	sz = nlepow2(round_up_to_page(sz));
	cbuf_mempool_resize(cos_spd_id(), 0);
	assert(0 == cbuf_debug_cbuf_info(cos_spd_id(), 2, 0));

	/* all cbufs in free list should be inconsistent */
	cbuf_map_collect(cos_spd_id());
	assert(0 == cbuf_collect(cos_spd_id(), sz));
	cbuf_mempool_resize(cos_spd_id(), sz);

	/* discard all inconsistent ones in free list */
	cbuf_alloc(sz, &cb);
	cbuf_vect_lookup_addr(cb);
	assert(0 == cbuf_collect(cos_spd_id(), sz));
	cbuf_free(cb);

	cbuf_mempool_resize(cos_spd_id(), 0);
	assert(0 == cbuf_collect(cos_spd_id(), sz));
	assert(0 == cbuf_debug_freelist_num(sz));
}

#endif /* CBUF_H */

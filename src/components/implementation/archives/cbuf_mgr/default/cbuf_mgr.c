/*
 * Copyright 2012 by The George Washington University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gparmer@gwu.edu, 2012
 */

#include <cos_component.h>
#include <mem_mgr_large.h>
#include <cbuf_meta.h>
#include <cbuf_mgr.h>
#include <cos_synchronization.h>
#include <valloc.h>
#include <sched.h>
#include <cos_alloc.h>
#include <cmap.h>
#include <cos_list.h>

#include <stkmgr.h>

#define DEBUG
#if defined(DEBUG)
int op_cnts[OP_NUM];
unsigned long long tsc_start[OP_NUM];
unsigned long long total_tsc_per_op[OP_NUM];
#endif

#define CB_DEF_POOL_SZ 4096*2048

/** 
 * The main data-structures tracked in this component.
 * 
 * cbuf_comp_info is the per-component data-structure that tracks the
 * page shared with the component to return garbage-collected cbufs, the
 * cbufs allocated to the component, and the data-structures for
 * tracking where the cbuf_metas are associated with the cbufs.
 * 
 * cbuf_meta_range is a simple linked list to track the metas for
 * given cbuf id ranges.
 *
 * cbuf_info is the per-cbuf structure that tracks the cbid, size,
 * and contains a linked list of all of the mappings for that cbuf.
 *
 * See the following diagram:

  cbuf_comp_info                 cbuf_meta_range
  +------------------------+	  +---------+	+---------+
  | spdid     	           |  +-->| daddr   +--->         |
  | +--------------------+ |  |	  | cbid    |	|         |
  | | size = X, c-+      | |  |	  | meta    <---+       | |
  | --------------|------- |  |	  +-|-------+	+-------|-+
  | | size = ...  |      | |  |     |  +-----------+	+-->
  | +-------------|------+ |  |	    +->|           |
  |          cbuf_metas-------+	       +-----^-----+ cbuf_meta
  +---------------|--------+	+------------|---+
		  |   		| cbid, size |   |
		  |     	| +----------|-+ | +------------+
		  +------------>| | spdid,.. | |<->| .., addr   |
		   		| +------------+ | +------------+
				+----------------+     cbuf_maps
                                cbuf_info
*/

/* Per-cbuf information */
struct cbuf_maps {
	spdid_t spdid;
	vaddr_t addr;
	struct cbuf_meta *m;
	struct cbuf_maps *next, *prev;
};

struct cbuf_info {
	unsigned int cbid;
	unsigned long size;
	char *mem;
	struct cbuf_maps owner;
	struct cbuf_info *next, *prev;
};

/* Per-component information */
struct cbuf_meta_range {
	struct cbuf_meta *m;
	vaddr_t dest;
	unsigned int low_id;
	struct cbuf_meta_range *next, *prev;
};
#define CBUF_META_RANGE_HIGH(cmr) (cmr->low_id + (int)(PAGE_SIZE/sizeof(struct cbuf_meta)))

struct cbuf_bin {
	unsigned long size;
	struct cbuf_info *c;
};

struct blocked_thd {
	unsigned short int thd_id;
	unsigned long request_size;
	struct blocked_thd *next, *prev;
	unsigned long long blk_start;
};

struct cbuf_tracking {
	unsigned long long gc_tot, gc_max, gc_start;
	unsigned long long blk_tot, blk_max;
	int gc_num;
};

struct cbuf_comp_info {
	spdid_t spdid;
	struct cbuf_shared_page *csp;
	vaddr_t dest_csp;
	int nbin;
	struct cbuf_bin cbufs[CBUF_MAX_NSZ];
	struct cbuf_meta_range *cbuf_metas;
	unsigned long target_size, allocated_size;
	int num_blocked_thds;
	struct blocked_thd bthd_list;
	struct cbuf_tracking track;
};

#define printl(s) //printc(s)
#if defined(DEBUG)
#define printd(...) printc("cbuf_mgr: "__VA_ARGS__)
#else
#define printd(...)
#endif

cos_lock_t cbuf_lock;
#define CBUF_LOCK_INIT() lock_static_init(&cbuf_lock);
#define CBUF_TAKE()      do { if (lock_take(&cbuf_lock))    BUG(); } while(0)
#define CBUF_RELEASE()   do { if (lock_release(&cbuf_lock)) BUG(); } while(0)
CVECT_CREATE_STATIC(components);
CMAP_CREATE_STATIC(cbufs);

static struct cbuf_info *cbi_head;

static inline void
tracking_init(void)
{
#if defined(DEBUG)
	memset(op_cnts, 0, sizeof(op_cnts));
	memset(total_tsc_per_op, 0, sizeof(total_tsc_per_op));
#endif
	return ;
}

static inline void
tracking_start(struct cbuf_tracking *t, cbuf_debug_t index)
{
#if defined(DEBUG)
	rdtscll(tsc_start[index]);
#endif

	if (t && index == CBUF_COLLECT) rdtscll(t->gc_start);
	return ;
}

static inline void
tracking_end(struct cbuf_tracking *t, cbuf_debug_t index)
{
	u64_t end, r;

	rdtscll(end);

#if defined(DEBUG)
	op_cnts[index]++;
	total_tsc_per_op[index] += (end-tsc_start[index]);
#endif

	if (t && index == CBUF_COLLECT) {
		t->gc_num++;
		r = end-t->gc_start;
		t->gc_tot += r;
		if (r > t->gc_max) t->gc_max = r;
	}
	return ;
}

static struct cbuf_meta_range *
cbuf_meta_lookup_cmr(struct cbuf_comp_info *comp, unsigned int cbid)
{
	struct cbuf_meta_range *cmr;
	assert(comp);

	cmr = comp->cbuf_metas;
	if (!cmr) return NULL;
	do {
		if (cmr->low_id <= cbid && CBUF_META_RANGE_HIGH(cmr) > cbid) {
			return cmr;
		}
		cmr = FIRST_LIST(cmr, next, prev);
	} while (cmr != comp->cbuf_metas);

	return NULL;
}

static struct cbuf_meta *
cbuf_meta_lookup(struct cbuf_comp_info *comp, unsigned int cbid)
{
	struct cbuf_meta_range *cmr;

	cmr = cbuf_meta_lookup_cmr(comp, cbid);
	if (!cmr) return NULL;
	return &cmr->m[cbid - cmr->low_id];
}

static struct cbuf_meta_range *
cbuf_meta_add(struct cbuf_comp_info *comp, unsigned int cbid, struct cbuf_meta *m, vaddr_t dest)
{
	struct cbuf_meta_range *cmr;

	if (cbuf_meta_lookup(comp, cbid)) return NULL;
	cmr = malloc(sizeof(struct cbuf_meta_range));
	if (unlikely(!cmr)) return NULL;
	INIT_LIST(cmr, next, prev);
	cmr->m      = m;
	cmr->dest   = dest;
	cmr->low_id = round_to_pow2(cbid, PAGE_SIZE/sizeof(struct cbuf_meta));

	if (comp->cbuf_metas) ADD_LIST(comp->cbuf_metas, cmr, next, prev);
	else                  comp->cbuf_metas = cmr;

	return cmr;
}

static void
cbuf_comp_info_init(spdid_t spdid, struct cbuf_comp_info *cci)
{
	void *p;

	memset(cci, 0, sizeof(*cci));
	cci->spdid       = spdid;
	cci->target_size = CB_DEF_POOL_SZ;
	INIT_LIST(&cci->bthd_list, next, prev);
	cvect_add(&components, cci, spdid);
}

static struct cbuf_comp_info *
cbuf_comp_info_get(spdid_t spdid)
{
	struct cbuf_comp_info *cci;

	cci = cvect_lookup(&components, spdid);
	if (!cci) {
		cci = malloc(sizeof(*cci));
		if (!cci) return NULL;
		cbuf_comp_info_init(spdid, cci);
	}
	return cci;
}

static struct cbuf_bin *
cbuf_comp_info_bin_get(struct cbuf_comp_info *cci, unsigned long sz)
{
	int i;

	assert(sz);
	for (i = 0 ; i < cci->nbin ; i++) {
		if (sz == cci->cbufs[i].size) return &cci->cbufs[i];
	}
	return NULL;
}

static struct cbuf_bin *
cbuf_comp_info_bin_add(struct cbuf_comp_info *cci, unsigned long sz)
{
	cci->cbufs[cci->nbin].size = sz;
	cci->nbin++;

	return &cci->cbufs[cci->nbin-1];
}

static int
cbuf_map(spdid_t spdid, vaddr_t daddr, void *page, unsigned long size, int flags)
{
	unsigned long off;
	assert(size == round_to_page(size));
	assert(daddr);
	assert(page);
	for (off = 0 ; off < size ; off += PAGE_SIZE) {
		vaddr_t d = daddr + off;
		if (unlikely(d != (mman_alias_page(cos_spd_id(), ((vaddr_t)page) + off,
						   spdid, d, flags)))) {
			for (d = daddr + off - PAGE_SIZE ; d >= daddr ; d -= PAGE_SIZE) {
				mman_revoke_page(spdid, d, 0);
			}
			return -ENOMEM;
		}
	}
	return 0;
}

/* 
 * map the memory from address p and size sz to the component spdid with 
 * permission flags. if p is NULL allocate a piece of new memory
 * return spdid's address to daddr, manager's virtual address to page
 */
static int
cbuf_alloc_map(spdid_t spdid, vaddr_t *daddr, void **page, void *p, unsigned long sz, int flags, vaddr_t exact_dest)
{
	vaddr_t dest;
	int ret = 0;
	void *new_p;

	tracking_start(NULL, CBUF_MAP);

	assert(sz == round_to_page(sz));
	if (!p) {
		new_p = page_alloc(sz/PAGE_SIZE);
		assert(new_p);
		memset(new_p, 0, sz);
	} else {
		new_p = p;
	}

	if (exact_dest) dest = exact_dest;
	else dest = (vaddr_t)valloc_alloc(cos_spd_id(), spdid, sz/PAGE_SIZE);
	
	if (unlikely(!dest)) goto free;
	if (!cbuf_map(spdid, dest, new_p, sz, flags)) goto done;

free: 
	if (dest) valloc_free(cos_spd_id(), spdid, (void *)dest, 1);
	if (!p) page_free(new_p, sz/PAGE_SIZE);
	ret = -1;

done:	
	if (page) *page  = new_p;
	*daddr = dest;
	tracking_end(NULL, CBUF_MAP);
	return ret;
}

/* 
 * Do any components have a reference to the cbuf? 
 * key function coordinates manager and client.
 * When this returns 1, this cbuf may or may not be used by some components.
 * When this returns 0, it guarantees: 
 * If all clients use the protocol correctly, there is no reference 
 * to the cbuf and no one will receive the cbuf after this. Furthermore, 
 * if the cbuf is in some free list, its inconsistent bit is already set.
 * That is to say, the manager can safely collect or re-balance this cbuf.
 *
 * Proof: 1. If a component gets the cbuf from free-list, it will 
 * simply discard this cbuf as its inconsistent bit is set.
 * 2. Assume component c sends the cbuf. 
 * It is impossible to send the cbuf after we check c's refcnt, since c 
 * has no reference to this cbuf.
 * If this send happens before we check c's refcnt, because the sum of 
 * nsent is equal to the sum of nrecv, this send has been received and 
 * no further receive will happen.
 * 3. No one will call cbuf2buf to receive this cbuf after this function, 
 * as all sends have been received and no more sends will occur during this function
 *
 * However, if clients do not use protocol correctly, this function 
 * provides no guarantee. cbuf_unmap_prepare takes care of this case.
 */
static int
cbuf_referenced(struct cbuf_info *cbi)
{
	struct cbuf_maps *m = &cbi->owner;
	int sent, recvd, ret = 1;
	unsigned long old_nfo, new_nfo;
	unsigned long long old;
	struct cbuf_meta *mt, *own_mt = m->m;

	old_nfo = own_mt->nfo;
	new_nfo = old_nfo | CBUF_INCONSISTENT;
	if (unlikely(!cos_cas(&own_mt->nfo, old_nfo, new_nfo))) goto done;

	mt   = (struct cbuf_meta *)(&old);
	sent = recvd = 0;
	do {
		struct cbuf_meta *meta = m->m;

		/* 
		 * Guarantee atomically read the two words (refcnt and nsent/nrecv).
		 * Consider this case, c0 sends a cbuf to c1 and frees this 
		 * this cbuf, but before c1 receives it, the manager comes in 
		 * and checks c1's refcnt. Now it is zero. But before the manager 
		 * checks c1's nsent/nrecv, it is preempted by c1. c1 receives 
		 * this cbuf--increment refcnt and nsent/nrecv. After this, we 
		 * switch back the manager, who will continues to check c1's 
		 * nsent/nrecv, now it is 1, which is equal to c0's nsent/nrecv. 
		 * Thus the manager can collect or unmap this cbuf.
		 */
		memcpy(&old, meta, sizeof(unsigned long long));
		if (unlikely(!cos_dcas(&old, old, old))) goto unset;
		if (CBUF_REFCNT(mt)) goto unset;		
		/* 
		 * TODO: add per-mapping counter of sent and recv in the manager
		 * each time atomic clear those counter in the meta
		 */
		sent  += mt->snd_rcv.nsent;
		recvd += mt->snd_rcv.nrecvd;
		m      = FIRST_LIST(m, next, prev);
	} while (m != &cbi->owner);
	if (sent != recvd) goto unset;
	ret = 0;
	if (CBUF_IS_IN_FREELIST(own_mt)) goto done;

unset:
	CBUF_FLAG_ATOMIC_REM(own_mt, CBUF_INCONSISTENT);
done:
	return ret;
}

/* TODO: Incorporate this into cbuf_referenced */
static void
cbuf_references_clear(struct cbuf_info *cbi)
{
	struct cbuf_maps *m = &cbi->owner;

	do {
		struct cbuf_meta *meta = m->m;

		if (meta) meta->snd_rcv.nsent = meta->snd_rcv.nrecvd = 0;
		m = FIRST_LIST(m, next, prev);
	} while (m != &cbi->owner);

	return;
}

/* 
 * Before actually unmap cbuf from a component, we need to atomically
 * clear the page pointer in the meta, which guarantees that clients 
 * do not have seg fault. Clients have to check NULL when receive cbuf
 */
static int
cbuf_unmap_prepare(struct cbuf_info *cbi)
{
	struct cbuf_maps *m = &cbi->owner;
	unsigned long old_nfo, new_nfo;

	if (cbuf_referenced(cbi)) return 1;
	cbuf_references_clear(cbi);

	/* 
	 * We need to clear out the meta. Consider here manager removes a
	 * cbuf from component c0 and allocates that cbuf to component c1,  
	 * but c1 sends the cbuf back to c0. If c0 sees the old meta, it may 
	 * be confused. However, we have to leave the inconsistent bit here
	 */
	do {
		old_nfo = m->m->nfo;
		if (old_nfo & CBUF_REFCNT_MAX) return 1;
		new_nfo = old_nfo & CBUF_INCONSISTENT;
		if (unlikely(!cos_cas(&m->m->nfo, old_nfo, new_nfo))) return 1;
		m   = FIRST_LIST(m, next, prev);
	} while (m != &cbi->owner);

	return 0;
}

/* 
 * As clients maybe malicious or don't use protocol correctly, we cannot 
 * simply unmap memory here. We guarantee that fault can only happen within 
 * the malicious component, but for other components, they either receive a 
 * NULL pointer from cbuf2buf or see wrong data. No fault happen in other 
 * components. See details in cbuf_unmap_prepare
 */
static int
cbuf_free_unmap(struct cbuf_comp_info *cci, struct cbuf_info *cbi)
{
	struct cbuf_maps *m = &cbi->owner, *next;
	struct cbuf_bin *bin;
	void *ptr = cbi->mem;
	unsigned long off, size = cbi->size;

	if (cbuf_unmap_prepare(cbi)) return 1;

	/* Unmap all of the pages from the clients */
	for (off = 0 ; off < size ; off += PAGE_SIZE) {
		mman_revoke_page(cos_spd_id(), (vaddr_t)ptr + off, 0);
	}

	/* 
	 * Deallocate the virtual address in the client, and cleanup
	 * the memory in this component
	 */
	m = FIRST_LIST(&cbi->owner, next, prev);
	while (m != &cbi->owner) {
		next = FIRST_LIST(m, next, prev);
		REM_LIST(m, next, prev);
		valloc_free(cos_spd_id(), m->spdid, (void*)m->addr, size/PAGE_SIZE);
		free(m);
		m = next;
	}
	valloc_free(cos_spd_id(), m->spdid, (void*)m->addr, size/PAGE_SIZE);

	/* deallocate/unlink our data-structures */
	page_free(ptr, size/PAGE_SIZE);
	cmap_del(&cbufs, cbi->cbid);
	cci->allocated_size -= size;
	bin = cbuf_comp_info_bin_get(cci, size);
	if (EMPTY_LIST(cbi, next, prev)) {
		bin->c = NULL;
	} else {
		if (bin->c == cbi) bin->c = cbi->next;
		REM_LIST(cbi, next, prev);
	}
	free(cbi);

	return 0;
}

static inline void
cbuf_mark_relinquish_all(struct cbuf_comp_info *cci)
{
	int i;
	struct cbuf_bin *bin;
	struct cbuf_info *cbi;
	struct cbuf_maps *m;

	for (i = cci->nbin-1 ; i >= 0 ; i--) {
		bin = &cci->cbufs[i];
		cbi = bin->c;
		do {
			if (!cbi) break;
			m = &cbi->owner;
			do {
				CBUF_FLAG_ATOMIC_ADD(m->m, CBUF_RELINQ);
				m  = FIRST_LIST(m, next, prev);
			} while(m != &cbi->owner);
			cbi   = FIRST_LIST(cbi, next, prev);
		} while (cbi != bin->c);
	}
}

static inline void
cbuf_unmark_relinquish_all(struct cbuf_comp_info *cci)
{
	int i;
	struct cbuf_bin *bin;
	struct cbuf_info *cbi;
	struct cbuf_maps *m;

	for(i=cci->nbin-1; i>=0; --i) {
		bin = &cci->cbufs[i];
		cbi = bin->c;
		do {
			if (!cbi) break;
			m = &cbi->owner;
			do {
				CBUF_FLAG_ATOMIC_REM(m->m, CBUF_RELINQ);
				m  = FIRST_LIST(m, next, prev);
			} while(m != &cbi->owner);
			cbi   = FIRST_LIST(cbi, next, prev);
		} while (cbi != bin->c);
	}
}

static inline void
cbuf_thread_block(struct cbuf_comp_info *cci, unsigned long request_size)
{
	struct blocked_thd bthd;

	bthd.thd_id       = cos_get_thd_id();
	bthd.request_size = request_size;
	rdtscll(bthd.blk_start);
	ADD_LIST(&cci->bthd_list, &bthd, next, prev);
	cci->num_blocked_thds++;
	cbuf_mark_relinquish_all(cci);
	CBUF_RELEASE();
	sched_block(cos_spd_id(), 0);
}

/* wake up all blocked threads whose request size smaller than or equal to available size */
static void
cbuf_thd_wake_up(struct cbuf_comp_info *cci, unsigned long sz)
{
	struct blocked_thd *bthd, *next;
	unsigned long long cur, tot;

	assert(cci->num_blocked_thds >= 0);
	/* Cannot wake up thd when in shrink */
	assert(cci->target_size >= cci->allocated_size);

	if (cci->num_blocked_thds == 0) return;
	bthd = cci->bthd_list.next;
	while (bthd != &cci->bthd_list) {
		next = FIRST_LIST(bthd, next, prev);
		if (bthd->request_size <= sz) {
			REM_LIST(bthd, next, prev);
			cci->num_blocked_thds--;
			rdtscll(cur);
			tot = cur-bthd->blk_start;
			cci->track.blk_tot += tot;
			if (tot > cci->track.blk_max) cci->track.blk_max = tot;
			sched_wakeup(cos_spd_id(), bthd->thd_id);
		}
		bthd = next;
	}
	if (cci->num_blocked_thds == 0) cbuf_unmark_relinquish_all(cci);
}

static void cbuf_shrink(struct cbuf_comp_info *cci, int diff);

int
__cbuf_create(spdid_t spdid, unsigned long size, int cbid, vaddr_t dest)
{
	struct cbuf_comp_info *cci;
	struct cbuf_info *cbi;
	struct cbuf_meta *meta;
	struct cbuf_bin *bin;
	int ret = 0;
	unsigned int id = (unsigned int) cbid;
	if (unlikely(cbid < 0)) return 0;

	tracking_start(NULL, CBUF_CRT);

	cci = cbuf_comp_info_get(spdid);
	if (unlikely(!cci)) goto done;

	/* 
	 * Client wants to allocate a new cbuf, but the meta might not
	 * be mapped in.
	 */
	if (!cbid) {
		/* TODO: check if have enough free memory: ask mem manager */
		/*memory usage exceeds the target, block this thread*/
		if (size + cci->allocated_size > cci->target_size) {
			cbuf_shrink(cci, size);
			if (size + cci->allocated_size > cci->target_size) {
				cbuf_thread_block(cci, size);
				return 0;
			}
		}

 		cbi = malloc(sizeof(struct cbuf_info));

		if (unlikely(!cbi)) goto done;
		/* Allocate and map in the cbuf. Discard inconsistent cbufs */
		/* TODO: Find a better way to manage those inconsistent cbufs */
		do {
			id   = cmap_add(&cbufs, cbi);
			meta = cbuf_meta_lookup(cci, id);
		} while(meta && CBUF_INCONSISTENT(meta));

		cbi->cbid        = id;
		size             = round_up_to_page(size);
		cbi->size        = size;
		cbi->owner.m     = NULL;
		cbi->owner.spdid = spdid;
		INIT_LIST(&cbi->owner, next, prev);
		INIT_LIST(cbi, next, prev);
	
		struct cbuf_info *cbi_new = (struct cbuf_info *) malloc(sizeof(struct cbuf_info));
		cbi_new->cbid = id;
		if (cbi_head) {
			ADD_END_LIST(cbi_head, cbi_new, next, prev);
		} else {
			cbi_head = cbi_new;
			INIT_LIST(cbi_head, next, prev);
		}
		
		if (cbuf_alloc_map(spdid, &(cbi->owner.addr), 
				   (void**)&(cbi->mem), NULL, size, MAPPING_RW, dest)) {
			goto free;
		}
	} 
	/* If the client has a cbid, then make sure we agree! */
	else {
		cbi = cmap_lookup(&cbufs, id);
		if (unlikely(!cbi)) goto done;
		if (unlikely(cbi->owner.spdid != spdid)) goto done;
	}
	meta = cbuf_meta_lookup(cci, id);

	/* We need to map in the meta for this cbid.  Tell the client. */
	if (!meta) {
		ret = (int)id * -1;
		goto done;
	}
	
	/* 
	 * Now we know we have a cbid, a backing structure for it, a
	 * component structure, and the meta mapped in for the cbuf.
	 * Update the meta with the correct addresses and flags!
	 */
	memset(meta, 0, sizeof(struct cbuf_meta));
	meta->sz            = cbi->size >> PAGE_ORDER;
	meta->cbid_tag.cbid = id;
	CBUF_FLAG_ADD(meta, CBUF_OWNER);
	CBUF_PTR_SET(meta, cbi->owner.addr);
	CBUF_REFCNT_INC(meta);

	/*
	 * When creates a new cbuf, the manager should be the only
	 * one who can access the meta
	 */
	/* TODO: malicious client may trigger this assertion, just for debug */
	assert(CBUF_REFCNT(meta) == 1);
	assert(CBUF_PTR(meta));
	cbi->owner.m = meta;

	/*
	 * Install cbi last. If not, after return a negative cbid, 
	 * collection may happen and get a dangle cbi
	 */
	bin = cbuf_comp_info_bin_get(cci, size);
	if (!bin) bin = cbuf_comp_info_bin_add(cci, size);
	if (unlikely(!bin)) goto free;
	if (bin->c) ADD_LIST(bin->c, cbi, next, prev);
	else        bin->c   = cbi;
	cci->allocated_size += size;
	ret = (int)id;
done:
	tracking_end(NULL, CBUF_CRT);
	return ret;
free:
	cmap_del(&cbufs, id);
	free(cbi);
	goto done;
}

int
cbuf_create(spdid_t spdid, unsigned long size, int cbid)
{
	int ret = 0;
	unsigned int id = (unsigned int)cbid;

	printl("cbuf_create\n");
	CBUF_TAKE();
	ret = __cbuf_create(spdid, size, cbid, 0);
	CBUF_RELEASE();

	return ret;
}

static inline struct cbuf_maps*
__cbuf_maps_create(spdid_t spdid, int cbid, int size, vaddr_t daddr)
{
	struct cbuf_comp_info *cci;
	struct cbuf_info *cbi; 
	struct cbuf_maps *map;
	vaddr_t dest = NULL;
	void *page;
	int ret = -EINVAL;

	cci        = cbuf_comp_info_get(spdid);
	if (!cci) goto done;
	cbi        = cmap_lookup(&cbufs, cbid);
	if (!cbi) goto done;
	/* shouldn't cbuf2buf your own buffer! */
	if (cbi->owner.spdid == spdid) goto done;

	map        = malloc(sizeof(struct cbuf_maps));
	if (!map) ERR_THROW(-ENOMEM, done);
	
	if (daddr) dest = daddr;

	map->spdid = spdid;
	map->m     = NULL;
	map->addr  = dest;
	INIT_LIST(map, next, prev);
	ADD_LIST(&cbi->owner, map, next, prev);

	page = cbi->mem;
	assert(page);

done:
	return map;
free:
	free(map);
	map = NULL;
	goto done;
}

vaddr_t
__cbuf_map_at(spdid_t s_spd, unsigned int cbid, spdid_t d_spd, vaddr_t d_addr)
{
	struct cbuf_info *cbi;
	int flags;
	vaddr_t ret = (vaddr_t)NULL;
	cbi = cmap_lookup(&cbufs, cbid);
	assert(cbi);
	if (unlikely(!cbi)) goto done;
	assert(cbi->owner.spdid == s_spd);
	/*
	 * the low-order bits of the d_addr are packed with the MAPPING flags (0/1)
	 * and a flag (2) set if valloc should not be used.
	 */
	flags = d_addr & 0x3;
	d_addr &= ~0x3;
	if (!(flags & MAPPING_NO_VALLOC) && 
	    valloc_alloc_at(s_spd, d_spd, (void*)d_addr, cbi->size/PAGE_SIZE)) goto done;
	if (cbuf_map(d_spd, d_addr, cbi->mem, cbi->size, flags & (MAPPING_READ|MAPPING_RW))) goto free;
	ret = d_addr;
	/*
	 * do not add d_spd to the meta list because the cbuf is not
	 * accessible directly. The s_spd must maintain the necessary info
	 * about the cbuf and its mapping in d_spd.
	 */

	if (!__cbuf_maps_create(d_spd, cbid, cbi->size, d_addr)) goto done;
done:
	return ret;
free:
	if (!(flags & MAPPING_NO_VALLOC)) valloc_free(s_spd, d_spd, (void*)d_addr, cbi->size);
	goto done;

}

vaddr_t
cbuf_map_at(spdid_t s_spd, unsigned int cbid, spdid_t d_spd, vaddr_t d_addr)
{
	vaddr_t ret = (vaddr_t)NULL;
	CBUF_TAKE();
	ret = __cbuf_map_at(s_spd, cbid, d_spd, d_addr);
	CBUF_RELEASE();
	return ret;
}

int
cbuf_unmap_at(spdid_t s_spd, unsigned int cbid, spdid_t d_spd, vaddr_t d_addr)
{
	struct cbuf_info *cbi;
	int ret = 0, err = 0;
	u32_t off;

	assert(d_addr);
	CBUF_TAKE();
	cbi = cmap_lookup(&cbufs, cbid);
	if (unlikely(!cbi)) ERR_THROW(-EINVAL, done);
	if (unlikely(cbi->owner.spdid != s_spd)) ERR_THROW(-EPERM, done);
	assert(cbi->size == round_to_page(cbi->size));
	/* unmap pages in only the d_spd client */
	for (off = 0 ; off < cbi->size ; off += PAGE_SIZE)
		err |= mman_release_page(d_spd, d_addr + off, 0);
	err |= valloc_free(s_spd, d_spd, (void*)d_addr, cbi->size/PAGE_SIZE);
	if (unlikely(err)) ERR_THROW(-EFAULT, done);
	assert(!err);
done:
	CBUF_RELEASE();
	return ret;
}

/*
 * Register the cbuf id with f_spd, at the same location it was registered in o_spd
 * returns d_addr on success, 0 on failure.
 *
 * Notice that this allocates a full page worth of metas so more than one meta. 1024 / 8 metas
 */
vaddr_t
__cbuf_register_map_at(spdid_t o_spd, spdid_t f_spd, long o_cbid, long f_cbid)
{
	struct cbuf_comp_info  *o_cci, *f_cci;
	struct cbuf_meta_range *o_cmr, *f_cmr;
	void *p;
	vaddr_t ret = 0;
	vaddr_t dest;

	o_cci = cbuf_comp_info_get(o_spd);
	if (!o_cci) goto done;
	f_cci = cbuf_comp_info_get(f_spd);
	if (!f_cci) goto done;
	o_cmr = cbuf_meta_lookup_cmr(o_cci, o_cbid);
	if (!o_cmr) goto done;
	f_cmr = cbuf_meta_lookup_cmr(f_cci, f_cbid);
	if (f_cmr) ERR_THROW(f_cmr->dest, done);
	
	/* Create the mapping into the client */
	if (cbuf_alloc_map(f_spd, &dest, &p, NULL, PAGE_SIZE, MAPPING_RW, o_cmr->dest)) goto done;
	assert((u32_t)p == round_to_page(p));
	f_cmr = cbuf_meta_add(f_cci, f_cbid, p, dest);
	assert(f_cmr);
	ret = f_cmr->dest;
	
done:
	return ret;
}

vaddr_t
__cbuf_fork_cbuf(spdid_t o_spd, unsigned int s_cbid, spdid_t f_spd, int copy_cinfo, spdid_t cbb_spd)
{
	struct cbuf_info *cbi;
	unsigned int sz;
	spdid_t spdid = f_spd;
	vaddr_t dest;
	int ret = -1;
	int f_cbid;
	int flags;
	struct cbuf_info *f_cbi;
	void *memcpy_ret;
	struct cbuf_maps *c;	/* c for current because we love one letter names */

	cbi        = cmap_lookup(&cbufs, s_cbid);
	if (!cbi) BUG();
	sz = cbi->size;

	/* 
	 * cmap_lookup returns original owner so need to recurse 
	 * through mappings until we find the one with spdid O 
	 */
	c = &cbi->owner;
	assert(c);
	while (c->spdid != o_spd) c = c->next; // what about infinite loops???
	assert(c->spdid == o_spd);
	dest = c->addr;

	/* create cbuf */
	f_cbid = __cbuf_create(spdid, sz, 0, dest);
	if (f_cbid < 0) { // only do these steps if initial create failed because of lack of map
		if (!__cbuf_register_map_at(o_spd, f_spd, cbi->cbid, f_cbid * -1)) goto done; 
		f_cbid = __cbuf_create(spdid, sz, f_cbid * -1, dest);
	}

	if (f_cbid <= 0) goto done;
	assert(f_cbid);

	/* copy into cbuf */
	assert(cbi->mem);
	f_cbi = cmap_lookup(&cbufs, f_cbid);
	if (!f_cbi) BUG(); 				/* we literally just made this in cbuf_create so if it's not there, something is very wrong */
	assert(f_cbi->mem);
	memcpy_ret = memcpy(f_cbi->mem, cbi->mem, sz);
	assert(memcpy_ret == f_cbi->mem);

#ifdef COS_DEBUG
	/* Do a sanity check and REMOVE THIS once we're kinda confident stuff works */
	if (memcmp(f_cbi->mem, cbi->mem, sz)) { printd("cbufs do not actually match. :(\n"); BUG(); }
#endif
	
	ret = 0;

	if (copy_cinfo) {
		vaddr_t q_daddr = (vaddr_t)valloc_alloc(cos_spd_id(), cbb_spd, sz/PAGE_SIZE);
		if (unlikely(!q_daddr)) return -1;
		flags = MAPPING_RW;
		flags |= MAPPING_NO_VALLOC;
		if (q_daddr != __cbuf_map_at(f_spd, f_cbid, cbb_spd, q_daddr | flags)) return -1;
		ret = q_daddr;
	}

done:
	return ret;

}

/*
 * This is meant to be silly code to help me (Teo) debug LockdownOS but maybe other people would like it too.
 * Prints out info relevant to component cci. Does NOT modify stuff.
 */
static void
__get_nfo(struct cbuf_comp_info *cci)
{
	printc("Now iterating over global cbuf list\n");
	// iterate over cbi_head until done
	struct cbuf_info *current = cbi_head;
	if (!current) {
		printc("There should be at least one cbuf in the system\n");
		BUG();
	}

	do {
		// The cbuf_info list we made is actually crap because it copies the pointers so if the structure is updated it is no longer valid (how to fix???)
		// instead, re-get the cbi from the cbuf_id which we assume is constant
		struct cbuf_info *cbi = cmap_lookup(&cbufs, current->cbid);

		if (cbi) { /* presumably the cbuf could have been deleted but this is unlikely */
			printc("cbuf #%2u with size %x, mem start %x, and maps ", cbi->cbid, cbi->size, cbi->mem);
			struct cbuf_maps *m = &cbi->owner;

			do {
				printc("[%d:%x] ", m->spdid, m->addr);
				m = FIRST_LIST(m, next, prev);
			} while (m != &cbi->owner);

			printc("\n");
		}

		current = FIRST_LIST(current, next, prev);
	} while (current != cbi_head);

	printc("Done iterating over global cbuf list\n");
}

/* This is internal so the real core of the function doesn't take a lock */
static vaddr_t
__cbuf_fork_spd(spdid_t cbb_spd, spdid_t o_spd, spdid_t f_spd, int cinfo_cbid)
{
	struct cbuf_comp_info *src, *dst;
	struct cbuf_info *current;
	int i;
	vaddr_t ret = (vaddr_t) NULL;		/* address of cinfo page */

	int r = valloc_fork_spd(cos_spd_id(), o_spd, f_spd);

	printc("forking all cbufs from %d to %d with cinfo spdid %d\n", o_spd, f_spd, cinfo_cbid);	
	src = cbuf_comp_info_get(o_spd);
	dst = cbuf_comp_info_get(f_spd);

	/* Should create a new shared page between cbuf_mgr and dst */
	dst->csp = NULL;
	
	current = cbi_head;
	vaddr_t r_addr;
	do {
		/* 
		 * The cbuf_info list we made is actually crap because it copies the pointers so if the structure is updated it is no longer valid (how to fix???)
		 * instead, re-get the cbi from the cbuf_id which we assume is constant
		 */
		struct cbuf_info *cbi = cmap_lookup(&cbufs, current->cbid);
		struct cbuf_maps *m = &cbi->owner;
	
		if (m->spdid == o_spd) {
			/* This is for if O is the owner */
			/* This just universally forks everything to a new cbuf. */
			__cbuf_fork_cbuf(o_spd, cbi->cbid, f_spd, 0, 0);
		}
		else {
			do {
				struct cbuf_meta *meta = m->m;
				/* This is if O isn't the owner but has the cbuf mapped in. */
				if (m->spdid == o_spd && !(meta && meta->cbid_tag.tag == 0)) {
					r_addr = __cbuf_fork_cbuf(o_spd, cbi->cbid, 
					                          f_spd, cinfo_cbid == cbi->cbid, cbb_spd);
					if (cinfo_cbid == cbi->cbid) ret = r_addr;
				}

				m = FIRST_LIST(m, next, prev);
			} while (m != &cbi->owner);
		}

		current = FIRST_LIST(current, next, prev);
	} while (current != cbi_head);

	__get_nfo(dst);

done:
	return ret;
}

vaddr_t
cbuf_fork_spd(spdid_t spd, spdid_t s_spd, spdid_t d_spd, int cinfo_cbid)
{
	vaddr_t ret = (vaddr_t) NULL;
	printl("cbuf_fork_spd\n");

	CBUF_TAKE();
	/* 
	 * This assumes the calling component is part of the CBBOOTER. 
	 * That seems reasonable for now. Likely won't be forever. 
	 */
	ret = __cbuf_fork_spd(spd, s_spd, d_spd, cinfo_cbid);
	CBUF_RELEASE();
	return ret;
}

/*
 * Allocate and map the garbage-collection list used for cbuf_collect()
 */
vaddr_t
cbuf_map_collect(spdid_t spdid)
{
	struct cbuf_comp_info *cci;
	vaddr_t ret = (vaddr_t)NULL;

	printl("cbuf_map_collect\n");
	CBUF_TAKE();
	cci = cbuf_comp_info_get(spdid);
	if (unlikely(!cci)) goto done;

	/* if the mapped page exists already, just return it. */
	if (cci->dest_csp) {
		ret = cci->dest_csp;
		goto done;
	}

	assert(sizeof(struct cbuf_shared_page) <= PAGE_SIZE);
	/* alloc/map is leaked. Where should it be freed/unmapped? */
	if (cbuf_alloc_map(spdid, &cci->dest_csp, (void**)&cci->csp, NULL, PAGE_SIZE, MAPPING_RW, 0)) {
		goto done;
	}
	ret = cci->dest_csp;

	/* initialize a continuous ck ring */
	assert(cci->csp->ring.size == 0);
	CK_RING_INIT(cbuf_ring, &cci->csp->ring, NULL, CSP_BUFFER_SIZE);

done:
	CBUF_RELEASE();
	return ret;
}

/*
 * For a certain principal, collect any unreferenced and not_in 
 * free list cbufs so that they can be reused.  This is the 
 * garbage-collection mechanism.
 *
 * Collect cbufs and add them onto the shared component's ring buffer.
 *
 * This function is semantically complicated. It can return no cbufs 
 * even if they are available to force the pool of cbufs to be
 * expanded (the client will call cbuf_create in this case). 
 * Or, the common case: it can return a number of available cbufs.
 */
int
cbuf_collect(spdid_t spdid, unsigned long size)
{
	struct cbuf_info *cbi;
	struct cbuf_comp_info *cci;
	struct cbuf_shared_page *csp;
	struct cbuf_bin *bin;
	int ret = 0;

	printl("cbuf_collect\n");

	CBUF_TAKE();
	cci  = cbuf_comp_info_get(spdid);
	tracking_start(&cci->track, CBUF_COLLECT);
	if (unlikely(!cci)) ERR_THROW(-ENOMEM, done);
	if (size + cci->allocated_size <= cci->target_size) goto done;

	csp  = cci->csp;
	if (unlikely(!csp)) ERR_THROW(-EINVAL, done);

	assert(csp->ring.size == CSP_BUFFER_SIZE);
	ret = CK_RING_SIZE(cbuf_ring, &csp->ring);
	if (ret != 0) goto done;
	/* 
	 * Go through all cbufs we own, and report all of them that
	 * have no current references to them.  Unfortunately, this is
	 * O(N*M), N = min(num cbufs, PAGE_SIZE/sizeof(int)), and M =
	 * num components.
	 */
	size = round_up_to_page(size);
	bin  = cbuf_comp_info_bin_get(cci, size);
	if (!bin) ERR_THROW(0, done);
	cbi  = bin->c;
	do {
		if (!cbi) break;
		/*
		 * skip cbufs which are in freelist. Coordinates with cbuf_free to 
		 * detect such cbufs correctly. 
		 * We must check refcnt first and then next pointer.
		 *
		 * If do not check refcnt: the manager may check "next" before cbuf_free 
		 * (when it is NULL), then switch to client who calls cbuf_free to set 
		 * "next", decrease refcnt and add cbuf to freelist. Then switch back to 
		 * manager, but now it will collect this in-freelist cbuf.
		 * 
		 * Furthermore we must check refcnt before the "next" pointer: 
		 * If not, similar to above case, the manager maybe preempted by client 
		 * between the manager checks "next" and refcnt. Therefore the manager 
		 * finds the "next" is null and refcnt is 0, and collect this cbuf.
		 * Short-circuit can prevent reordering. 
		 */
		assert(cbi->owner.m);
		if (!CBUF_REFCNT(cbi->owner.m) && !CBUF_IS_IN_FREELIST(cbi->owner.m)
                 		    && !cbuf_referenced(cbi)) {
			struct cbuf_ring_element el = { .cbid = cbi->cbid };
			cbuf_references_clear(cbi);
			if (!CK_RING_ENQUEUE_SPSC(cbuf_ring, &csp->ring, &el)) break;
			/*
			 * Prevent other collection collecting those cbufs.
			 * The manager checks if the shared ring buffer is empty upon 
			 * the entry, if not, it just returns. This is not enough to 
			 * prevent double-collection. The corner case is: 
			 * after the last one in ring buffer is dequeued and 
			 * before it is added to the free-list, the manager  
			 * appears. It may collect the last one again.
			 */
			cbi->owner.m->next = (struct cbuf_meta *)1;
			if (++ret == CSP_BUFFER_SIZE) break;
		}
		cbi = FIRST_LIST(cbi, next, prev);
	} while (cbi != bin->c);
	if (ret) cbuf_thd_wake_up(cci, ret*size);

done:
	tracking_end(&cci->track, CBUF_COLLECT);
	CBUF_RELEASE();
	return ret;
}

/* 
 * Called by cbuf_deref.
 */
int
cbuf_delete(spdid_t spdid, unsigned int cbid)
{
	struct cbuf_comp_info *cci;
	struct cbuf_info *cbi;
	struct cbuf_meta *meta;
	int ret = -EINVAL, sz;

	printl("cbuf_delete\n");
	CBUF_TAKE();
	tracking_start(NULL, CBUF_DEL);

	cci  = cbuf_comp_info_get(spdid);
	if (unlikely(!cci)) goto done;
	cbi  = cmap_lookup(&cbufs, cbid);
	if (unlikely(!cbi)) goto done;
	meta = cbuf_meta_lookup(cci, cbid);

	/*
	 * Other threads can access the meta data simultaneously. For
	 * example, others call cbuf2buf which increase the refcnt.
	 */
	CBUF_REFCNT_ATOMIC_DEC(meta);
	/* Find the owner of this cbuf */
	if (cbi->owner.spdid != spdid) {
		cci = cbuf_comp_info_get(cbi->owner.spdid);
		if (unlikely(!cci)) goto done;
	}
	if (cbuf_free_unmap(cci, cbi)) 	goto done;
	if (cci->allocated_size < cci->target_size) {
		cbuf_thd_wake_up(cci, cci->target_size - cci->allocated_size);
	}
	ret = 0;
done:
	tracking_end(NULL, CBUF_DEL);
	CBUF_RELEASE();
	return ret;
}

/* 
 * Called by cbuf2buf to retrieve a given cbid.
 */
int
cbuf_retrieve(spdid_t spdid, unsigned int cbid, unsigned long size)
{
	struct cbuf_comp_info *cci, *own;
	struct cbuf_info *cbi;
	struct cbuf_meta *meta, *own_meta;
	struct cbuf_maps *map;
	void *page;
	int ret = -EINVAL, off;

	printl("cbuf_retrieve\n");

	CBUF_TAKE();
	tracking_start(NULL, CBUF_RETRV);

	cci        = cbuf_comp_info_get(spdid);
	if (!cci) goto done;
	cbi        = cmap_lookup(&cbufs, cbid);
	if (!cbi) goto done;
	meta       = cbuf_meta_lookup(cci, cbid);
	if (!meta) goto done;
	assert(!(meta->nfo & ~CBUF_INCONSISTENT));
	map = __cbuf_maps_create(spdid, cbid, size, NULL);
	if (!map) goto done;
	
	/* TODO: change to MAPPING_READ */
	if (size > cbi->size) goto done;
	assert((int)round_to_page(cbi->size) == cbi->size);
	size       = cbi->size;
	if (cbuf_alloc_map(spdid, &map->addr, NULL, cbi->mem, size, MAPPING_RW, 0)) {
		printc("cbuf mgr map fail spd %d mem %p sz %lu cbid %u\n", spdid, cbi->mem, size, cbid);
		goto free;
	}
	CBUF_PTR_SET(meta, map->addr);
	meta->sz            = cbi->size >> PAGE_ORDER;
	meta->cbid_tag.cbid = cbid;
	own                 = cbuf_comp_info_get(cbi->owner.spdid);
	if (unlikely(!own)) goto done;
	
	/*
	 * We need to inherit the relinquish bit from the sender. 
	 * Otherwise, this cbuf cannot be returned to the manager. 
	 */
	own_meta            = cbuf_meta_lookup(own, cbid);
	if (CBUF_RELINQ(own_meta)) CBUF_FLAG_ADD(meta, CBUF_RELINQ);
	ret                 = 0;
done:
	tracking_end(NULL, CBUF_RETRV);

	CBUF_RELEASE();
	return ret;
free:
	free(map);
	goto done;
}

vaddr_t
__cbuf_register(spdid_t spdid, long cbid)
{
	struct cbuf_comp_info  *cci;
	struct cbuf_meta_range *cmr;
	void *p;
	vaddr_t dest, ret = 0;

	printl("cbuf_register\n");
	tracking_start(NULL, CBUF_REG);

	cci = cbuf_comp_info_get(spdid);
	if (unlikely(!cci)) goto done;
	cmr = cbuf_meta_lookup_cmr(cci, cbid);
	if (cmr) ERR_THROW(cmr->dest, done);

	/* Create the mapping into the client */
	if (cbuf_alloc_map(spdid, &dest, &p, NULL, PAGE_SIZE, MAPPING_RW, 0)) goto done;
	assert((unsigned int)p == round_to_page(p));
	cmr = cbuf_meta_add(cci, cbid, p, dest);
	assert(cmr);
	ret = cmr->dest;
done:
	tracking_end(NULL, CBUF_REG);
	return ret;

}

vaddr_t
cbuf_register(spdid_t spdid, unsigned int cbid)
{
	vaddr_t ret = 0;
	CBUF_TAKE();
	ret = __cbuf_register(spdid, cbid);
	CBUF_RELEASE();
	return ret;
}

static void
cbuf_shrink(struct cbuf_comp_info *cci, int diff)
{
	int i, sz;
	struct cbuf_bin *bin;
	struct cbuf_info *cbi, *next, *head;

	for (i = cci->nbin-1 ; i >= 0 ; i--) {
		bin = &cci->cbufs[i];
		sz = (int)bin->size;
		if (!bin->c) continue;
		cbi = FIRST_LIST(bin->c, next, prev);
		while (cbi != bin->c) {
			next = FIRST_LIST(cbi, next, prev);
			if (!cbuf_free_unmap(cci, cbi)) {
				diff -= sz;
				if (diff <= 0) return;
			}
			cbi = next;
		}
		if (!cbuf_free_unmap(cci, cbi)) {
			diff -= sz;
			if (diff <= 0) return;
		}
	}
	if (diff > 0) cbuf_mark_relinquish_all(cci);
}

static inline void
cbuf_expand(struct cbuf_comp_info *cci, int diff)
{
	if (cci->allocated_size < cci->target_size) {
		cbuf_thd_wake_up(cci, cci->target_size - cci->allocated_size);
	}
}

/* target_size is an absolute size */
void
cbuf_mempool_resize(spdid_t spdid, unsigned long target_size)
{
	struct cbuf_comp_info *cci;
	int diff;

	CBUF_TAKE();
	cci = cbuf_comp_info_get(spdid);
	if (unlikely(!cci)) goto done;
	target_size = round_up_to_page(target_size);
	diff = (int)(target_size - cci->target_size);
	cci->target_size = target_size;
	if (diff < 0 && cci->allocated_size > cci->target_size) {
		cbuf_shrink(cci, cci->allocated_size - cci->target_size);
	}
	if (diff > 0) cbuf_expand(cci, diff);
done:
	CBUF_RELEASE();
}

unsigned long
cbuf_memory_target_get(spdid_t spdid)
{
	struct cbuf_comp_info *cci;
	int ret;
	CBUF_TAKE();
	cci = cbuf_comp_info_get(spdid);
	if (unlikely(!cci)) ERR_THROW(-ENOMEM, done);
	ret = cci->target_size;
done:
	CBUF_RELEASE();
	return ret;
}

/* the assembly code that invokes stkmgr expects this memory layout */
struct cos_stk {
	struct cos_stk *next;
	u32_t flags;
	u32_t thdid_owner;
	u32_t cpu_id;
} __attribute__((packed));
#define D_COS_STK_ADDR(d_addr) (d_addr + PAGE_SIZE - sizeof(struct cos_stk))

/* Never give up! */
void
stkmgr_return_stack(spdid_t s_spdid, vaddr_t addr)
{
	BUG();
}

/* map a stack into d_spdid.
 * TODO: use cbufs. */
void *
stkmgr_grant_stack(spdid_t d_spdid)
{
	struct cbuf_comp_info *cci;
	void *p, *ret = NULL;
	vaddr_t d_addr;

	printl("stkmgr_grant_stack (cbuf)\n");

	CBUF_TAKE();
	cci = cbuf_comp_info_get(d_spdid);
	if (!cci) goto done;

	if (cbuf_alloc_map(d_spdid, &d_addr, (void**)&p, NULL, PAGE_SIZE, MAPPING_RW, 0)) goto done;
	ret = (void*)D_COS_STK_ADDR(d_addr);

done:
	CBUF_RELEASE();
	return ret;
}

void
cos_init(void)
{
	CBUF_LOCK_INIT();
	cmap_init_static(&cbufs);
	cmap_add(&cbufs, NULL);

	tracking_init();
}


/* Debug helper functions */
static int 
__debug_reference(struct cbuf_info * cbi)
{
	struct cbuf_maps *m = &cbi->owner;
	int sent = 0, recvd = 0;

	do {
		struct cbuf_meta *meta = m->m;

		if (CBUF_REFCNT(meta)) return 1;
		sent  += meta->snd_rcv.nsent;
		recvd += meta->snd_rcv.nrecvd;
		m      = FIRST_LIST(m, next, prev);
	} while (m != &cbi->owner);
	if (sent != recvd) return 1;

	return 0;
}

unsigned long
cbuf_debug_cbuf_info(spdid_t spdid, int index, int p)
{
	unsigned long ret[20], sz;
	struct cbuf_comp_info *cci;
	struct cbuf_bin *bin;
	struct cbuf_info *cbi, *next, *head;
	struct cbuf_meta *meta;
	struct blocked_thd *bthd;
	unsigned long long cur;
	int i;

	CBUF_TAKE();
	cci = cbuf_comp_info_get(spdid);
	if (unlikely(!cci)) assert(0);
	memset(ret, 0, sizeof(ret));

	ret[CBUF_TARGET] = cci->target_size;
	ret[CBUF_ALLOC] = cci->allocated_size;

	for (i = cci->nbin-1 ; i >= 0 ; i--) {
		bin = &cci->cbufs[i];
		sz = bin->size;
		if (!bin->c) continue;
		cbi = bin->c;
		do {
			if (__debug_reference(cbi)) ret[CBUF_USE] += sz;
			else                        ret[CBUF_GARBAGE] += sz;
			meta = cbi->owner.m;
			if (CBUF_RELINQ(meta)) ret[CBUF_RELINQ_NUM]++;
			cbi = FIRST_LIST(cbi, next, prev);
		} while(cbi != bin->c);
	}
	assert(ret[CBUF_USE]+ret[CBUF_GARBAGE] == ret[CBUF_ALLOC]);

	ret[BLK_THD_NUM] = cci->num_blocked_thds;
	if (ret[BLK_THD_NUM]) {
		rdtscll(cur);
		bthd = cci->bthd_list.next;
		while (bthd != &cci->bthd_list) {
			cci->track.blk_tot += (cur-bthd->blk_start);
			ret[CBUF_BLK] += bthd->request_size;
			bthd->blk_start = cur;
			bthd = FIRST_LIST(bthd, next, prev);
		}
	}

	ret[TOT_BLK_TSC] = (unsigned long)cci->track.blk_tot;
	ret[MAX_BLK_TSC] = (unsigned long)cci->track.blk_max;
	ret[TOT_GC_TSC]  = (unsigned long)cci->track.gc_tot;
	ret[MAX_GC_TSC]  = (unsigned long)cci->track.gc_max;
	if (p == 1) {
		printc("target %lu %lu allocate %lu %lu\n", 
			ret[CBUF_TARGET], ret[CBUF_TARGET]/PAGE_SIZE, ret[CBUF_ALLOC], ret[CBUF_ALLOC]/PAGE_SIZE);
		printc("using %lu %lu garbage %lu %lu relinq %lu\n", ret[CBUF_USE], ret[CBUF_USE]/PAGE_SIZE, 
			ret[CBUF_GARBAGE], ret[CBUF_GARBAGE]/PAGE_SIZE, ret[CBUF_RELINQ_NUM]);
		printc("spd %d %lu thd blocked request %d pages %d\n", 
			spdid, ret[BLK_THD_NUM], ret[CBUF_BLK], ret[CBUF_BLK]/PAGE_SIZE);
		printc("spd %d blk_tot %lu blk_max %lu gc_tot %lu gc_max %lu\n", spdid, ret[TOT_BLK_TSC], 
			ret[MAX_BLK_TSC], ret[TOT_GC_TSC], ret[MAX_GC_TSC]);
	}
	if (p == 2) {
		cci->track.blk_tot = cci->track.blk_max = cci->track.gc_tot = cci->track.gc_max = 0;
		cci->track.gc_num = 0;
	}

	CBUF_RELEASE();
	return ret[index];
}

void cbuf_debug_cbiddump(unsigned int cbid)
{
	struct cbuf_info *cbi;
	struct cbuf_maps *m;

	printc("mgr dump cbid %u\n", cbid);
	cbi = cmap_lookup(&cbufs, cbid);
	assert(cbi);
	printc("cbid %u cbi: id %d sz %lu mem %p\n", cbid, cbi->cbid, cbi->size, cbi->mem);
	m = &cbi->owner;
	do {
		struct cbuf_meta *meta = m->m;
		printc("map: spd %d addr %lux meta %p\n", m->spdid, m->addr, m->m);
		printc("meta: nfo %lux addr %lux cbid %u\n", meta->nfo, CBUF_PTR(meta), meta->cbid_tag.cbid);
		m = FIRST_LIST(m, next, prev);
	} while(m != &cbi->owner);
}

void
cbuf_debug_profile(int p)
{
#if defined(DEBUG)
	int i;

	if (p) {
		for (i = 0 ; i < OP_NUM ; i++) {
			if (!op_cnts[i]) continue;
			switch(i) {
			case CBUF_CRT:
				printc("create %d avg %llu\n", op_cnts[i], total_tsc_per_op[i]/op_cnts[i]);
				break;
			case CBUF_COLLECT:
				printc("collect %d avg %llu\n", op_cnts[i], total_tsc_per_op[i]/op_cnts[i]);
				break;
			case CBUF_DEL:
				printc("delete %d avg %llu\n", op_cnts[i], total_tsc_per_op[i]/op_cnts[i]);
				break;
			case CBUF_RETRV:
				printc("retrieve %d avg %llu\n", op_cnts[i], total_tsc_per_op[i]/op_cnts[i]);
				break;
			case CBUF_REG:
				printc("register %d avg %llu\n", op_cnts[i], total_tsc_per_op[i]/op_cnts[i]);
				break;
			case CBUF_MAP:
				printc("alloc map %d avg %llu\n", op_cnts[i], total_tsc_per_op[i]/op_cnts[i]);
				break;
			default:
				break;
			}
		}
	}
	tracking_init();
#endif
	return ;
}

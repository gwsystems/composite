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
#include <cbuf.h>
#include <cbuf_mgr.h>
#include <cos_synchronization.h>
#include <valloc.h>
#include <sched.h>
#include <cos_alloc.h>
#include <cmap.h>
#include <cos_list.h>

#define INIT_LIMIT_SIZE 40960
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
	u32_t cbid;
	int size;
	char *mem;
	struct cbuf_maps owner;
	struct cbuf_info *next, *prev;
};

/* Per-component information */
struct cbuf_meta_range {
	struct cbuf_meta *m;
	vaddr_t dest;
	u32_t low_id;
	struct cbuf_meta_range *next, *prev;
};
#define CBUF_META_RANGE_HIGH(cmr) (cmr->low_id + (PAGE_SIZE/sizeof(struct cbuf_meta)))

struct cbuf_bin {
	int size;
	struct cbuf_info *c;
};
struct blocked_thd {
	unsigned short int thd_id;
	//spdid_t spdid;
	int request_size;
	struct blocked_thd *next, *prev;
};
struct cbuf_comp_info {
	spdid_t spdid;
	struct cbuf_shared_page *csp;
	vaddr_t dest_csp;
	int nbin;
	struct cbuf_bin cbufs[CBUF_MAX_NSZ];
	struct cbuf_meta_range *cbuf_metas;
	int limit_size, allocated_size, desired_size;
	struct blocked_thd bthd_list;
};

#define printl(s) //printc(s)
cos_lock_t cbuf_lock;
#define CBUF_LOCK_INIT() lock_static_init(&cbuf_lock);
#define CBUF_TAKE()      do { if (lock_take(&cbuf_lock))    BUG(); } while(0)
#define CBUF_RELEASE()   do { if (lock_release(&cbuf_lock)) BUG(); } while(0)
CVECT_CREATE_STATIC(components);
CMAP_CREATE_STATIC(cbufs);

static struct cbuf_meta_range *
cbuf_meta_lookup_cmr(struct cbuf_comp_info *comp, u32_t cbid)
{
	struct cbuf_meta_range *cmr;
	assert(comp);

	cmr = comp->cbuf_metas;
	if (!cmr) return NULL;
	do {
		if (cmr->low_id >= cbid || CBUF_META_RANGE_HIGH(cmr) > cbid) {
			return cmr;
		}
		cmr = FIRST_LIST(cmr, next, prev);
	} while (cmr != comp->cbuf_metas);

	return NULL;
}

static struct cbuf_meta *
cbuf_meta_lookup(struct cbuf_comp_info *comp, u32_t cbid)
{
	struct cbuf_meta_range *cmr;

	cmr = cbuf_meta_lookup_cmr(comp, cbid);
	if (!cmr) return NULL;
	return &cmr->m[cbid - cmr->low_id];
}

static struct cbuf_meta_range *
cbuf_meta_add(struct cbuf_comp_info *comp, u32_t cbid, struct cbuf_meta *m, vaddr_t dest)
{
	struct cbuf_meta_range *cmr;

	if (cbuf_meta_lookup(comp, cbid)) return NULL;
	cmr = malloc(sizeof(struct cbuf_meta_range));
	if (!cmr) return NULL;
	INIT_LIST(cmr, next, prev);
	cmr->m      = m;
	cmr->dest   = dest;
	/* must be power of 2: */
	cmr->low_id = (cbid & ~((PAGE_SIZE/sizeof(struct cbuf_meta))-1));

	if (comp->cbuf_metas) ADD_LIST(comp->cbuf_metas, cmr, next, prev);
	else                  comp->cbuf_metas = cmr;

	return cmr;
}

static void
cbuf_comp_info_init(spdid_t spdid, struct cbuf_comp_info *cci)
{
	memset(cci, 0, sizeof(*cci));
	cci->spdid = spdid;
	INIT_LIST(&cci->bthd_list, next, prev);
	cci->limit_size = INIT_LIMIT_SIZE;
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
cbuf_comp_info_bin_get(struct cbuf_comp_info *cci, int sz)
{
	int i;

	assert(sz);
	for (i = 0 ; i < cci->nbin ; i++) {
		if (sz == cci->cbufs[i].size) return &cci->cbufs[i];
	}
	return NULL;
}

static struct cbuf_bin *
cbuf_comp_info_bin_add(struct cbuf_comp_info *cci, int sz)
{
	if (sz == CBUF_MAX_NSZ) return NULL;
	cci->cbufs[cci->nbin].size = sz;
	cci->nbin++;

	return &cci->cbufs[cci->nbin-1];
}

static int
cbuf_map(spdid_t spdid, vaddr_t daddr, void *page, int size, int flags)
{
	int off;
	assert(size == (int)round_to_page(size));
	assert(daddr);
	assert(page);
	for (off = 0 ; off < size ; off += PAGE_SIZE) {
		vaddr_t d = daddr + off;
		if (d != (mman_alias_page(cos_spd_id(), ((vaddr_t)page) + off,
						spdid, d, flags))) {
			assert(0); /* TODO: roll back the aliases, etc... */
		}
	}
	return 0;
}

static int
cbuf_alloc_map(spdid_t spdid, vaddr_t *daddr, void **page, int size)
{
	void *p;
	vaddr_t dest;
	int ret;

	assert(size == (int)round_to_page(size));
	p = page_alloc(size/PAGE_SIZE);
	assert(p);
	memset(p, 0, size);

	dest = (vaddr_t)valloc_alloc(cos_spd_id(), spdid, size/PAGE_SIZE);
	assert(dest);

	ret = cbuf_map(spdid, dest, p, size, MAPPING_RW);
	if (ret) valloc_free(cos_spd_id(), spdid, (void *)daddr, 1);
	*page  = p;
	*daddr = dest;

	return ret;
}

static inline void
cbuf_add_to_blk_list(struct cbuf_comp_info *cci, int request_size)
{
	struct blocked_thd *bthd;
	bthd = malloc(sizeof(struct blocked_thd));
	if (unlikely(bthd == NULL)) BUG();
	bthd->thd_id = cos_get_thd_id();
	//bthd->spdid = cci->spdid;
	bthd->request_size = request_size;
	ADD_LIST(&cci->bthd_list, bthd, next, prev);
}

/* Do any components have a reference to the cbuf? */
static int
cbuf_referenced(struct cbuf_info *cbi)
{
	struct cbuf_maps *m = &cbi->owner;
	int sent, recvd;

	sent = recvd = 0;
	do {
		struct cbuf_meta *meta = m->m;

		if (meta) {
			if (CBUFM_GET_REFCNT(meta)) return 1;
			sent  += meta->owner_nfo.c.nsent;
			recvd += meta->owner_nfo.c.nrecvd;
		}

		m = FIRST_LIST(m, next, prev);
	} while (m != &cbi->owner);
	if (sent != recvd) return 1;
	
	return 0;
}

static void
cbuf_references_clear(struct cbuf_info *cbi)
{
	struct cbuf_maps *m = &cbi->owner;

	do {
		struct cbuf_meta *meta = m->m;

		if (meta) {
			meta->owner_nfo.c.nsent = meta->owner_nfo.c.nrecvd = 0;
		}
		m = FIRST_LIST(m, next, prev);
	} while (m != &cbi->owner);

	return;
}

static void
cbuf_free_unmap(struct cbuf_comp_info *cci, struct cbuf_info *cbi)
{
	struct cbuf_maps *m = &cbi->owner;
	struct cbuf_bin *bin;
	struct cbuf_meta *in_free = NULL;
	void *ptr = cbi->mem;
	int off;

	if (cbuf_referenced(cbi)) assert(0);
	cbuf_references_clear(cbi);
	in_free = (struct cbuf_meta *)CBUFM_GET_NEXT(m->m);
	do {
		assert(m->m);
		assert(!CBUFM_GET_REFCNT(m->m));
		/* TODO: fix race here with atomic instruction */
		memset(m->m, 0, sizeof(struct cbuf_meta));

		m = FIRST_LIST(m, next, prev);
	} while (m != &cbi->owner);
	/*this cbuf is in freelist, set it as inconsistent*/
	if (in_free) CBUF_UNSET_INCONSISENT(cbi->owner.m);
	/* Unmap all of the pages from the clients */
	for (off = 0 ; off < cbi->size ; off += PAGE_SIZE) {
		mman_revoke_page(cos_spd_id(), (vaddr_t)ptr + off, 0);
	}

	/* 
	 * Deallocate the virtual address in the client, and cleanup
	 * the memory in this component
	 */
	m = &cbi->owner;
	do {
		struct cbuf_maps *next;

		next = FIRST_LIST(m, next, prev);
		REM_LIST(m, next, prev);
		valloc_free(cos_spd_id(), m->spdid, (void*)m->addr, cbi->size/PAGE_SIZE);
		if (m != &cbi->owner) free(m);
		m = next;
	} while (m != &cbi->owner);

	/* deallocate/unlink our data-structures */
	page_free(ptr, cbi->size/PAGE_SIZE);
	cmap_del(&cbufs, cbi->cbid);
	cci->allocated_size -= cbi->size;
	bin = cbuf_comp_info_bin_get(cci, cbi->size);
	if (EMPTY_LIST(cbi, next, prev)) {
		bin->c = NULL;
		--cci->nbin;
	}
	else {
		if (bin->c == cbi) bin->c = cbi->next;
		REM_LIST(cbi, next, prev);
	}
	free(cbi);
}

int
cbuf_create(spdid_t spdid, int size, long cbid)
{
	struct cbuf_comp_info *cci;
	struct cbuf_info *cbi;
	struct cbuf_meta *meta;
	int ret = 0;

	printl("cbuf_create\n");
	if (unlikely(cbid < 0)) return 0;
	CBUF_TAKE();
	cci = cbuf_comp_info_get(spdid);
	if (!cci) goto done;

	/* 
	 * Client wants to allocate a new cbuf, but the meta might not
	 * be mapped in.
	 */
	if (!cbid) {
		/*memory usage exceeds the limit, block this thread*/
		if (size + cci->allocated_size > cci->limit_size) {
			cbuf_add_to_blk_list(cci, size);
			CBUF_RELEASE();
			sched_block(cos_spd_id(), 0);
			assert(size + cci->allocated_size <= cci->limit_size);
			return 0;
		}
		struct cbuf_bin *bin;

 		cbi = malloc(sizeof(struct cbuf_info));
		if (!cbi) goto done;

		/* Allocate and map in the cbuf. */
		cbid = cmap_add(&cbufs, cbi);
		cbi->cbid        = cbid;
		size             = round_up_to_page(size);
		cbi->size        = size;
		cbi->owner.m     = NULL;
		cbi->owner.spdid = spdid;
		INIT_LIST(&cbi->owner, next, prev);
		INIT_LIST(cbi, next, prev);

		bin = cbuf_comp_info_bin_get(cci, size);
		if (!bin) bin = cbuf_comp_info_bin_add(cci, size);
		if (!bin) goto free;

		if (cbuf_alloc_map(spdid, &(cbi->owner.addr), 
				    (void**)&(cbi->mem), size)) goto free;
		if (bin->c) ADD_LIST(bin->c, cbi, next, prev);
		else        bin->c = cbi;
		cci->allocated_size += size;
	} 
	/* If the client has a cbid, then make sure we agree! */
	else {
		cbi = cmap_lookup(&cbufs, cbid);
		if (!cbi) goto done;
		if (cbi->owner.spdid != spdid) goto done;
	}
	meta = cbuf_meta_lookup(cci, cbid);
	/* We need to map in the meta for this cbid.  Tell the client. */
	if (!meta) {
		ret = cbid * -1;
		goto done;
	}
	cbi->owner.m = meta;

	/* 
	 * Now we know we have a cbid, a backing structure for it, a
	 * component structure, and the meta mapped in for the cbuf.
	 * Update the meta with the correct addresses and flags!
	 */
	memset(meta, 0, sizeof(struct cbuf_meta));
	CBUF_SET_OWNER(meta);
	CBUF_SET_WRITABLE(meta);
	CBUF_SET_TOUCHED(meta);
	CBUFM_SET_PTR(meta, cbi->owner.addr);
	meta->sz        = cbi->size >> PAGE_ORDER;
	meta->cbid.cbid = cbid;
	assert(CBUFM_GET_REFCNT(meta) < CBUF_REFCNT_MAX);
	CBUFM_INC_REFCNT(meta);
	ret = cbid;
done:
	CBUF_RELEASE();

	return ret;
free:
	cmap_del(&cbufs, cbid);
	free(cbi);
	goto done;
}

vaddr_t
cbuf_map_at(spdid_t s_spd, cbuf_t cbid, spdid_t d_spd, vaddr_t d_addr, int flags)
{
	vaddr_t ret = (vaddr_t)NULL;
	struct cbuf_info *cbi;
	u32_t id;
	int tmem;
	
	cbuf_unpack(cbid, &id);
	assert(tmem == 0);
	CBUF_TAKE();
	cbi = cmap_lookup(&cbufs, id);
	assert(cbi);
	if (unlikely(!cbi)) goto done;
	assert(cbi->owner.spdid == s_spd);
	if (valloc_alloc_at(s_spd, d_spd, (void*)d_addr, cbi->size/PAGE_SIZE)) goto done;
	if (cbuf_map(d_spd, d_addr, cbi->mem, cbi->size, flags)) goto free;
	ret = d_addr;
	/* do not add d_spd to the meta list because the cbuf is not
	 * accessible directly. The s_spd must maintain the necessary info
	 * about the cbuf and its mapping in d_spd. */
done:
	CBUF_RELEASE();
	return ret;
free:
	//valloc_free(s_spd, d_spd, d_addr, cbi->size);
	goto done;
}

int
cbuf_unmap_at(spdid_t s_spd, cbuf_t cbid, spdid_t d_spd, vaddr_t d_addr)
{
	struct cbuf_info *cbi;
	int off;
	int ret = 0;
	u32_t id;
	int tmem;
	int err;
	
	cbuf_unpack(cbid, &id);
	assert(tmem == 0);

	assert(d_addr);
	CBUF_TAKE();
	cbi = cmap_lookup(&cbufs, id);
	if (unlikely(!cbi)) ERR_THROW(-EINVAL, done);
	if (unlikely(cbi->owner.spdid != s_spd)) ERR_THROW(-EINVAL, done);
	assert(cbi->size == (int)round_to_page(cbi->size));
	/* unmap pages in only the d_spd client */
	for (off = 0 ; off < cbi->size ; off += PAGE_SIZE)
		mman_release_page(d_spd, d_addr + off, 0);
	err = valloc_free(s_spd, d_spd, (void*)d_addr, cbi->size/PAGE_SIZE);
	if (unlikely(err)) ERR_THROW(-EFAULT, done);
	assert(!err);
done:
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
	if (cbuf_alloc_map(spdid, &cci->dest_csp, (void**)&cci->csp, PAGE_SIZE)) goto done;
	ret = cci->dest_csp;

	/* initialize a continuous ck ring */
	assert(cci->csp->ring.size == 0);
	CK_RING_INIT(cbuf_ring, &cci->csp->ring, NULL, CSP_BUFFER_SIZE);

done:
	CBUF_RELEASE();
	return ret;
}

/*
 * For a certain principal, collect any unreferenced persistent cbufs
 * so that they can be reused.  This is the garbage-collection
 * mechanism.
 *
 * Collect cbufs and add them onto the component's freelist.
 *
 * This function is semantically complicated.  It can block if no
 * cbufs are available, and the component is not supposed to allocate
 * any more.  It can return no cbufs even if they are available to
 * force the pool of cbufs to be expanded (the client will call
 * cbuf_create in this case).  Or, the common case: it can return a
 * number of available cbufs.
 */
int
cbuf_collect(spdid_t spdid, int size)
{
	struct cbuf_info *cbi;
	struct cbuf_comp_info *cci;
	struct cbuf_shared_page *csp;
	struct cbuf_bin *bin;
	int ret = 0;

	printl("cbuf_collect\n");

	CBUF_TAKE();
	cci = cbuf_comp_info_get(spdid);
	if (unlikely(!cci)) ERR_THROW(-ENOMEM, done);
	csp = cci->csp;
	if (unlikely(!csp)) ERR_THROW(-EINVAL, done);

	assert(csp->ring.size == CSP_BUFFER_SIZE);
	/* 
	 * Go through all cbufs we own, and report all of them that
	 * have no current references to them.  Unfortunately, this is
	 * O(N*M), N = min(num cbufs, PAGE_SIZE/sizeof(int)), and M =
	 * num components.
	 */
	bin = cbuf_comp_info_bin_get(cci, round_up_to_page(size));
	if (!bin) ERR_THROW(0, done);
	cbi = bin->c;
	do {
		if (!cbi) break;
		/*skip cbufs which are in freelist*/
		if (!cbufm_is_in_freelist(cbi->owner.m) && !cbuf_referenced(cbi)) {
			struct cbuf_ring_element el = { .cbid = cbi->cbid };
			cbuf_references_clear(cbi);
			if (!CK_RING_ENQUEUE_SPSC(cbuf_ring, &csp->ring, &el)) break;
			if (++ret == CSP_BUFFER_SIZE) break;
		}
		cbi = FIRST_LIST(cbi, next, prev);
	} while (cbi != bin->c);
done:
	CBUF_RELEASE();
	return ret;
}

static inline void
cbuf_mark_relinquish_all(struct cbuf_comp_info *cci)
{
	int i;
	struct cbuf_bin *bin;
	struct cbuf_info *cbi;

	for(i=cci->nbin-1; i>=0; --i) {
		bin = &cci->cbufs[i];
		cbi = bin->c;
		do {
			CBUF_SET_RELINQ(cbi->owner.m);
			cbi = FIRST_LIST(cbi, next, prev);
		} while (cbi != bin->c);
	}
}
static inline void
cbuf_unmark_relinquish_all(struct cbuf_comp_info *cci)
{
	int i;
	struct cbuf_bin *bin;
	struct cbuf_info *cbi;

	for(i=cci->nbin-1; i>=0; --i) {
		bin = &cci->cbufs[i];
		cbi = bin->c;
		do {
			CBUF_UNSET_RELINQ(cbi->owner.m);
			cbi = FIRST_LIST(cbi, next, prev);
		} while (cbi != bin->c);
	}
}
/* 
 * Called by cbuf_deref.
 */
int
cbuf_delete(spdid_t spdid, int cbid)
{
	struct cbuf_comp_info *cci;
	struct cbuf_info *cbi;
	int ret = -EINVAL, sz;

	printl("cbuf_delete\n");
	CBUF_TAKE();
	cci = cbuf_comp_info_get(spdid);
	if (!cci) goto done;
	cbi = cmap_lookup(&cbufs, cbid);
	if (!cbi) goto done;
	sz = cbi->size;
	cbuf_free_unmap(cci, cbi);
	if (cci->desired_size > 0 && cci->desired_size <= sz) cbuf_unmark_relinquish_all(cci);
	cci->desired_size = cci->desired_size > sz ? cci->desired_size - sz : 0;
	ret = 0;
done:
	CBUF_RELEASE();
	return ret;
}

/* 
 * Called by cbuf2buf to retrieve a given cbid.
 */
int
cbuf_retrieve(spdid_t spdid, int cbid, int size)
{
	struct cbuf_comp_info *cci;
	struct cbuf_info *cbi;
	struct cbuf_meta *meta;
	struct cbuf_maps *map;
	vaddr_t dest;
	void *page;
	int ret = -EINVAL, off;

	printl("cbuf_retrieve\n");

	CBUF_TAKE();
	cci        = cbuf_comp_info_get(spdid);
	if (!cci) goto done;
	cbi        = cmap_lookup(&cbufs, cbid);
	if (!cbi) goto done;
	/* shouldn't cbuf2buf your own buffer! */
	if (cbi->owner.spdid == spdid) goto done;
	meta       = cbuf_meta_lookup(cci, cbid);
	if (!meta) goto done;

	map        = malloc(sizeof(struct cbuf_maps));
	if (!map) ERR_THROW(-ENOMEM, done);
	if (size > cbi->size) goto done;
	assert((int)round_to_page(cbi->size) == cbi->size);
	size       = cbi->size;
	dest       = (vaddr_t)valloc_alloc(cos_spd_id(), spdid, size/PAGE_SIZE);
	if (!dest) goto free;

	map->spdid = spdid;
	map->m     = meta;
	map->addr  = dest;
	INIT_LIST(map, next, prev);
	ADD_LIST(&cbi->owner, map, next, prev);

	page = cbi->mem;
	assert(page);
	if (cbuf_map(spdid, dest, page, size, MAPPING_READ))
		valloc_free(cos_spd_id(), spdid, (void *)dest, 1);
	memset(meta, 0, sizeof(struct cbuf_meta));
	CBUF_SET_TOUCHED(meta);
	CBUFM_SET_PTR(meta, map->addr);
	meta->sz        = cbi->size >> PAGE_ORDER;
	meta->cbid.cbid = cbid;
	ret             = 0;
done:
	CBUF_RELEASE();
	return ret;
free:
	free(map);
	goto done;
}

vaddr_t
cbuf_register(spdid_t spdid, long cbid)
{
	struct cbuf_comp_info  *cci;
	struct cbuf_meta_range *cmr;
	void *p;
	vaddr_t dest, ret = 0;

	printl("cbuf_register\n");
	CBUF_TAKE();
	cci = cbuf_comp_info_get(spdid);
	if (!cci) goto done;
	cmr = cbuf_meta_lookup_cmr(cci, cbid);
	if (cmr) ERR_THROW(cmr->dest, done);

	/* Create the mapping into the client */
	if (cbuf_alloc_map(spdid, &dest, &p, PAGE_SIZE)) goto done;
	assert((u32_t)p == round_to_page(p));
	cmr = cbuf_meta_add(cci, cbid, p, dest);
	assert(cmr);
	ret = cmr->dest;
done:
	CBUF_RELEASE();
	return ret;
}

static void
cbuf_shrink(struct cbuf_comp_info  *cci, int diff)
{
	int i, sz;
	struct cbuf_bin *bin;
	struct cbuf_info *cbi, *next, *head;
	/*last shrinking doesn't finish*/
	if (cci->desired_size) {
		cci->desired_size += diff;
		return ;
	}
	for(i=cci->nbin-1; i>=0; --i) {
		bin = &cci->cbufs[i];
		sz = bin->size;
		head = cbi = bin->c;
		do {
			next = FIRST_LIST(cbi, next, prev);
			if (!cbuf_referenced(cbi)) {
				cbuf_free_unmap(cci, cbi);
				diff -= sz;
				if (diff <= 0) return;
			}
			cbi = next;
		} while (cbi != head);
	}
	if (diff > 0) {
		cci->desired_size = diff;
		cbuf_mark_relinquish_all(cci);
	}
}
void 
cbuf_resize_mempool(spdid_t spdid, int diff)
{
	struct cbuf_comp_info  *cci;
	CBUF_TAKE();
	cci = cbuf_comp_info_get(spdid);
	if (!cci) goto done;
	cci->limit_size += diff;
	if (diff < 0) cbuf_shrink(cci, -1*diff);
done:
	CBUF_RELEASE();
	return ;

}
void
cos_init(void)
{
	long cbid;
	CBUF_LOCK_INIT();
	cmap_init_static(&cbufs);
	cbid = cmap_add(&cbufs, NULL);
}

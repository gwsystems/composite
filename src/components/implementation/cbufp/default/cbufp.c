/*
 * Copyright 2012 by The George Washington University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gparmer@gwu.edu, 2012
 */

#include <cos_component.h>
#include <cbuf_meta.h>
#include <mem_mgr_large.h>
#include <cbuf.h>
#include <cbufp.h>
#include <cbuf_c.h>
#include <cos_synchronization.h>
#include <valloc.h>
#include <cos_alloc.h>
#include <cmap.h>
#include <cos_list.h>

/** 
 * The main data-structures tracked in this component.
 * 
 * cbufp_comp_info is the per-component data-structure that tracks the
 * page shared with the component to return garbage-collected cbufs, the
 * cbufs allocated to the component, and the data-structures for
 * tracking where the cbuf_metas are associated with the cbufs.
 * 
 * cbuf_meta_range is a simple linked list to track the metas for
 * given cbuf id ranges.
 *
 * cbufp_info is the per-cbuf structure that tracks the cbid, size,
 * and contains a linked list of all of the mappings for that cbuf.
 *
 * See the following diagram:

  cbufp_comp_info                 cbuf_meta_range
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
				+----------------+     cbufp_maps
                                cbufp_info
*/

/* Per-cbuf information */
struct cbufp_maps {
	spdid_t spdid;
	vaddr_t addr;
	struct cbuf_meta *m;
	struct cbufp_maps *next, *prev;
};

struct cbufp_info {
	u32_t cbid;
	int size;
	char *mem;
	struct cbufp_maps owner;
	struct cbufp_info *next, *prev;
};

/* Per-component information */
struct cbufp_meta_range {
	struct cbuf_meta *m;
	vaddr_t dest;
	u32_t low_id;
	struct cbufp_meta_range *next, *prev;
};
#define CBUFP_META_RANGE_HIGH(cmr) (cmr->low_id + (PAGE_SIZE/sizeof(struct cbuf_meta)))

struct cbufp_bin {
	int size;
	struct cbufp_info *c;
};

struct cbufp_comp_info {
	spdid_t spdid;
	struct cbufp_shared_page *csp;
	vaddr_t dest_csp;
	int nbin;
	struct cbufp_bin cbufs[CBUFP_MAX_NSZ];
	struct cbufp_meta_range *cbuf_metas;
};

#define printl(s) //printc(s)
cos_lock_t cbufp_lock;
#define CBUFP_LOCK_INIT() lock_static_init(&cbufp_lock);
#define CBUFP_TAKE()      do { if (lock_take(&cbufp_lock))    BUG(); } while(0)
#define CBUFP_RELEASE()   do { if (lock_release(&cbufp_lock)) BUG(); } while(0)
CVECT_CREATE_STATIC(components);
CMAP_CREATE_STATIC(cbufs);

static struct cbufp_meta_range *
cbufp_meta_lookup_cmr(struct cbufp_comp_info *comp, u32_t cbid)
{
	struct cbufp_meta_range *cmr;
	assert(comp);

	cmr = comp->cbuf_metas;
	if (!cmr) return NULL;
	do {
		if (cmr->low_id >= cbid || CBUFP_META_RANGE_HIGH(cmr) > cbid) {
			return cmr;
		}
		cmr = FIRST_LIST(cmr, next, prev);
	} while (cmr != comp->cbuf_metas);

	return NULL;
}

static struct cbuf_meta *
cbufp_meta_lookup(struct cbufp_comp_info *comp, u32_t cbid)
{
	struct cbufp_meta_range *cmr;

	cmr = cbufp_meta_lookup_cmr(comp, cbid);
	if (!cmr) return NULL;
	return &cmr->m[cbid - cmr->low_id];
}

static struct cbufp_meta_range *
cbufp_meta_add(struct cbufp_comp_info *comp, u32_t cbid, struct cbuf_meta *m, vaddr_t dest)
{
	struct cbufp_meta_range *cmr;

	if (cbufp_meta_lookup(comp, cbid)) return NULL;
	cmr = malloc(sizeof(struct cbufp_meta_range));
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
cbufp_comp_info_init(spdid_t spdid, struct cbufp_comp_info *cci)
{
	memset(cci, 0, sizeof(*cci));
	cci->spdid = spdid;
	cvect_add(&components, cci, spdid);
}

static struct cbufp_comp_info *
cbufp_comp_info_get(spdid_t spdid)
{
	struct cbufp_comp_info *cci;

	cci = cvect_lookup(&components, spdid);
	if (!cci) {
		cci = malloc(sizeof(*cci));
		if (!cci) return NULL;
		cbufp_comp_info_init(spdid, cci);
	}
	return cci;
}

static struct cbufp_bin *
cbufp_comp_info_bin_get(struct cbufp_comp_info *cci, int sz)
{
	int i;

	assert(sz);
	for (i = 0 ; i < cci->nbin ; i++) {
		if (sz == cci->cbufs[i].size) return &cci->cbufs[i];
	}
	return NULL;
}

static struct cbufp_bin *
cbufp_comp_info_bin_add(struct cbufp_comp_info *cci, int sz)
{
	if (sz == CBUFP_MAX_NSZ) return NULL;
	cci->cbufs[cci->nbin].size = sz;
	cci->nbin++;

	return &cci->cbufs[cci->nbin-1];
}

static int
cbufp_map(spdid_t spdid, vaddr_t daddr, void *page, int size, int flags)
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
cbufp_alloc_map(spdid_t spdid, vaddr_t *daddr, void **page, int size)
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

	ret = cbufp_map(spdid, dest, p, size, MAPPING_RW);
	if (ret) valloc_free(cos_spd_id(), spdid, (void *)daddr, 1);
	*page  = p;
	*daddr = dest;

	return ret;
}

/* Do any components have a reference to the cbuf? */
static int
cbufp_referenced(struct cbufp_info *cbi)
{
	struct cbufp_maps *m = &cbi->owner;
	int sent, recvd;

	sent = recvd = 0;
	do {
		struct cbuf_meta *meta = m->m;

		if (meta) {
			if (meta->nfo.c.refcnt) return 1;
			sent  += meta->owner_nfo.c.nsent;
			recvd += meta->owner_nfo.c.nrecvd;
		}

		m = FIRST_LIST(m, next, prev);
	} while (m != &cbi->owner);

	if (sent != recvd) return 1;
	
	return 0;
}

static void
cbufp_references_clear(struct cbufp_info *cbi)
{
	struct cbufp_maps *m = &cbi->owner;

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
cbufp_free_unmap(spdid_t spdid, struct cbufp_info *cbi)
{
	struct cbufp_maps *m = &cbi->owner;
	void *ptr = cbi->mem;
	int off;

	if (cbufp_referenced(cbi)) return;
	cbufp_references_clear(cbi);
	do {
		assert(m->m);
		assert(!m->m->nfo.c.refcnt);
		/* TODO: fix race here with atomic instruction */
		memset(m->m, 0, sizeof(struct cbuf_meta));

		m = FIRST_LIST(m, next, prev);
	} while (m != &cbi->owner);

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
		struct cbufp_maps *next;

		next = FIRST_LIST(m, next, prev);
		REM_LIST(m, next, prev);
		valloc_free(cos_spd_id(), m->spdid, (void*)m->addr, cbi->size/PAGE_SIZE);
		if (m != &cbi->owner) free(m);
		m = next;
	} while (m != &cbi->owner);

	/* deallocate/unlink our data-structures */
	page_free(ptr, cbi->size/PAGE_SIZE);
	cmap_del(&cbufs, cbi->cbid);
	free(cbi);
}

int
cbufp_create(spdid_t spdid, int size, long cbid)
{
	struct cbufp_comp_info *cci;
	struct cbufp_info *cbi;
	struct cbuf_meta *meta;
	int ret = 0;

	printl("cbufp_create\n");
	if (unlikely(cbid < 0)) return 0;
	CBUFP_TAKE();
	cci = cbufp_comp_info_get(spdid);
	if (!cci) goto done;

	/* 
	 * Client wants to allocate a new cbuf, but the meta might not
	 * be mapped in.
	 */
	if (!cbid) {
		struct cbufp_bin *bin;

 		cbi = malloc(sizeof(struct cbufp_info));
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

		bin = cbufp_comp_info_bin_get(cci, size);
		if (!bin) bin = cbufp_comp_info_bin_add(cci, size);
		if (!bin) goto free;

		if (cbufp_alloc_map(spdid, &(cbi->owner.addr), 
				    (void**)&(cbi->mem), size)) goto free;
		if (bin->c) ADD_LIST(bin->c, cbi, next, prev);
		else        bin->c = cbi;
	} 
	/* If the client has a cbid, then make sure we agree! */
	else {
		cbi = cmap_lookup(&cbufs, cbid);
		if (!cbi) goto done;
		if (cbi->owner.spdid != spdid) goto done;
	}
	meta = cbufp_meta_lookup(cci, cbid);
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
	meta->nfo.c.flags |= CBUFM_TOUCHED | 
		             CBUFM_OWNER  | CBUFM_WRITABLE;
	meta->nfo.c.ptr    = cbi->owner.addr >> PAGE_ORDER;
	meta->sz           = cbi->size >> PAGE_ORDER;
	if (meta->nfo.c.refcnt == CBUFP_REFCNT_MAX) assert(0);
	meta->nfo.c.refcnt++;
	ret = cbid;
done:
	CBUFP_RELEASE();

	return ret;
free:
	cmap_del(&cbufs, cbid);
	free(cbi);
	goto done;
}

vaddr_t
cbufp_map_at(spdid_t s_spd, cbufp_t cbid, spdid_t d_spd, vaddr_t d_addr, int flags)
{
	vaddr_t ret = (vaddr_t)NULL;
	struct cbufp_info *cbi;
	u32_t id;
	int tmem;
	
	cbuf_unpack(cbid, &id, &tmem);
	assert(tmem == 0);
	CBUFP_TAKE();
	cbi = cmap_lookup(&cbufs, id);
	assert(cbi);
	if (unlikely(!cbi)) goto done;
	assert(cbi->owner.spdid == s_spd);
	//if (valloc_alloc_at(s_spd, d_spd, d_addr, cbi->size)) goto done;
	if (cbufp_map(d_spd, d_addr, cbi->mem, cbi->size, flags)) goto free;
	ret = d_addr;
	/* do not add d_spd to the meta list because the cbufp is not
	 * accessible directly. The s_spd must maintain the necessary info
	 * about the cbufp and its mapping in d_spd. */
done:
	CBUFP_RELEASE();
	return ret;
free:
	//valloc_free(s_spd, d_spd, d_addr, cbi->size);
	goto done;
}

int
cbufp_unmap_at(spdid_t s_spd, cbufp_t cbid, spdid_t d_spd, vaddr_t d_addr)
{
	struct cbufp_info *cbi;
	int off;
	int ret = 0;
	u32_t id;
	int tmem;
	
	cbuf_unpack(cbid, &id, &tmem);
	assert(tmem == 0);

	assert(d_addr);
	CBUFP_TAKE();
	cbi = cmap_lookup(&cbufs, id);
	if (unlikely(!cbi)) ERR_THROW(-EINVAL, done);
	if (unlikely(cbi->owner.spdid != s_spd)) ERR_THROW(-EINVAL, done);
	assert(cbi->size == (int)round_to_page(cbi->size));
	/* unmap pages in only the d_spd client */
	for (off = 0 ; off < cbi->size ; off += PAGE_SIZE)
		mman_release_page(d_spd, d_addr + off, 0);
done:
	CBUFP_RELEASE();
	return ret;
}

/*
 * Allocate and map the garbage-collection list used for cbufp_collect()
 */
vaddr_t
cbufp_map_collect(spdid_t spdid)
{
	struct cbufp_comp_info *cci;
	vaddr_t ret = (vaddr_t)NULL;

	printl("cbufp_map_collect\n");

	CBUFP_TAKE();
	cci = cbufp_comp_info_get(spdid);
	if (unlikely(!cci)) goto done;

	/* if the mapped page exists already, just return it. */
	if (cci->dest_csp) {
		ret = cci->dest_csp;
		goto done;
	}

	assert(sizeof(struct cbufp_shared_page) <= PAGE_SIZE);
	/* alloc/map is leaked. Where should it be freed/unmapped? */
	if (cbufp_alloc_map(spdid, &cci->dest_csp, (void**)&cci->csp, PAGE_SIZE)) goto done;
	ret = cci->dest_csp;

	/* initialize a continuous ck ring */
	assert(cci->csp->ring.size == 0);
	CK_RING_INIT(cbufp_ring, &cci->csp->ring, NULL, CSP_BUFFER_SIZE);

done:
	CBUFP_RELEASE();
	return ret;
}

/*
 * For a certain principal, collect any unreferenced persistent cbufs
 * so that they can be reused.  This is the garbage-collection
 * mechanism.
 *
 * Collect cbufps and add them onto the component's freelist.
 *
 * This function is semantically complicated.  It can block if no
 * cbufps are available, and the component is not supposed to allocate
 * any more.  It can return no cbufps even if they are available to
 * force the pool of cbufps to be expanded (the client will call
 * cbufp_create in this case).  Or, the common case: it can return a
 * number of available cbufs.
 */
int
cbufp_collect(spdid_t spdid, int size)
{
	struct cbufp_info *cbi;
	struct cbufp_comp_info *cci;
	struct cbufp_shared_page *csp;
	struct cbufp_bin *bin;
	int ret = 0;

	printl("cbufp_collect\n");

	CBUFP_TAKE();
	cci = cbufp_comp_info_get(spdid);
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
	bin = cbufp_comp_info_bin_get(cci, round_up_to_page(size));
	if (!bin) ERR_THROW(0, done);
	cbi = bin->c;
	do {
		if (!cbi) break;
		if (!cbufp_referenced(cbi)) {
			struct cbufp_ring_element el = { .cbid = cbi->cbid };
			cbufp_references_clear(cbi);
			if (!CK_RING_ENQUEUE_SPSC(cbufp_ring, &csp->ring, &el)) break;
			if (++ret == CSP_BUFFER_SIZE) break;
		}
		cbi = FIRST_LIST(cbi, next, prev);
	} while (cbi != bin->c);
done:
	CBUFP_RELEASE();
	return ret;
}

/* 
 * Called by cbufp_deref.
 */
int
cbufp_delete(spdid_t spdid, int cbid)
{
	struct cbufp_comp_info *cci;
	struct cbufp_info *cbi;
	int ret = -EINVAL;

	printl("cbufp_delete\n");
	assert(0);
	CBUFP_TAKE();
	cci = cbufp_comp_info_get(spdid);
	if (!cci) goto done;
	cbi = cmap_lookup(&cbufs, cbid);
	if (!cbi) goto done;
	
	cbufp_free_unmap(spdid, cbi);
	ret = 0;
done:
	CBUFP_RELEASE();
	return ret;
}

/* 
 * Called by cbufp2buf to retrieve a given cbid.
 */
int
cbufp_retrieve(spdid_t spdid, int cbid, int size)
{
	struct cbufp_comp_info *cci;
	struct cbufp_info *cbi;
	struct cbuf_meta *meta;
	struct cbufp_maps *map;
	vaddr_t dest;
	void *page;
	int ret = -EINVAL, off;

	printl("cbufp_retrieve\n");

	CBUFP_TAKE();
	cci        = cbufp_comp_info_get(spdid);
	if (!cci) goto done;
	cbi        = cmap_lookup(&cbufs, cbid);
	if (!cbi) goto done;
	/* shouldn't cbuf2buf your own buffer! */
	if (cbi->owner.spdid == spdid) goto done;
	meta       = cbufp_meta_lookup(cci, cbid);
	if (!meta) goto done;

	map        = malloc(sizeof(struct cbufp_maps));
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
	if (cbufp_map(spdid, dest, page, size, MAPPING_READ))
		valloc_free(cos_spd_id(), spdid, (void *)dest, 1);

	meta->nfo.c.flags |= CBUFM_TOUCHED;
	meta->nfo.c.ptr    = map->addr >> PAGE_ORDER;
	meta->sz           = cbi->size >> PAGE_ORDER;
	ret                = 0;
done:
	CBUFP_RELEASE();

	return ret;
free:
	free(map);
	goto done;
}

vaddr_t
cbufp_register(spdid_t spdid, long cbid)
{
	struct cbufp_comp_info  *cci;
	struct cbufp_meta_range *cmr;
	void *p;
	vaddr_t dest, ret = 0;

	printl("cbufp_register\n");
	CBUFP_TAKE();
	cci = cbufp_comp_info_get(spdid);
	if (!cci) goto done;
	cmr = cbufp_meta_lookup_cmr(cci, cbid);
	if (cmr) ERR_THROW(cmr->dest, done);

	/* Create the mapping into the client */
	if (cbufp_alloc_map(spdid, &dest, &p, PAGE_SIZE)) goto done;
	assert((u32_t)p == round_to_page(p));
	cmr = cbufp_meta_add(cci, cbid, p, dest);
	assert(cmr);
	ret = cmr->dest;
done:
	CBUFP_RELEASE();
	return ret;
}

void
cos_init(void)
{
	long cbid;
	CBUFP_LOCK_INIT();
	cmap_init_static(&cbufs);
	cbid = cmap_add(&cbufs, NULL);
}

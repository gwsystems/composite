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
	struct cbufp_bin cbufs;
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

static struct cbufp_comp_info *
cbufp_comp_info_get(spdid_t spdid)
{
	struct cbufp_comp_info *cci;

	cci = cvect_lookup(&components, spdid);
	if (!cci) {
		cci = malloc(sizeof(struct cbufp_comp_info));
		if (!cci) return NULL;
		memset(cci, 0, sizeof(struct cbufp_comp_info));
		cci->spdid = spdid;
		cvect_add(&components, cci, spdid);
	}
	return cci;
}

static int
cbufp_alloc_map(spdid_t spdid, vaddr_t *daddr, void **page, int size)
{
	void *p;
	vaddr_t dest;

	assert(size == PAGE_SIZE);
	dest = (vaddr_t)valloc_alloc(cos_spd_id(), spdid, 1);
	assert(dest);
	p = alloc_page();
	assert(p);
	memset(p, 0, PAGE_SIZE);
	if (dest != (mman_alias_page(cos_spd_id(), (vaddr_t)p, spdid, dest))) {
		assert(0);
		valloc_free(cos_spd_id(), spdid, (void *)dest, 1);
	}
	*page  = p;
	*daddr = dest;

	return 0;
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
			if (meta->nfo.c.flags & CBUFM_IN_USE) {
				return 1;
			}
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
	int size;

	if (cbufp_referenced(cbi)) return;

	do {
		struct cbufp_maps *next;

		next = FIRST_LIST(m, next, prev);
		REM_LIST(m, next, prev);
		valloc_free(cos_spd_id(), m->spdid, (void*)m->addr, cbi->size/PAGE_SIZE);
		m = next;
	} while (m != &cbi->owner);

	/* TODO: iterate through the size, and free all... */
	mman_revoke_page(cos_spd_id(), (vaddr_t)ptr, 0);
	//free_page(ptr);
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
 		cbi = malloc(sizeof(struct cbufp_info));
		if (!cbi) goto done;

		/* Allocate and map in the cbuf. */
		cbid = cmap_add(&cbufs, cbi);
		cbi->cbid        = cbid;
		cbi->size        = size;
		cbi->owner.m     = NULL;
		cbi->owner.spdid = spdid;
		INIT_LIST(&cbi->owner, next, prev);
		INIT_LIST(cbi, next, prev);
		if (cbufp_alloc_map(spdid, &(cbi->owner.addr), 
				    (void**)&(cbi->mem), size)) goto free;
		if (cci->cbufs.c) ADD_LIST(cci->cbufs.c, cbi, next, prev);
		else              cci->cbufs.c = cbi;
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
	meta->nfo.c.flags |= CBUFM_IN_USE | CBUFM_TOUCHED | 
		             CBUFM_OWNER  | CBUFM_WRITABLE;
	meta->nfo.c.ptr    = cbi->owner.addr >> PAGE_ORDER;
	ret = cbid;
done:
	CBUFP_RELEASE();

	return ret;
free:
	cmap_del(&cbufs, cbid);
	free(cbi);
	goto done;
}

/*
 * For a certain principal, collect any unreferenced persistent cbufs
 * so that they can be reused.  This is the garbage-collection
 * mechanism.
 *
 * Collect cbufps and add them onto the component's freelist.
 */
int
cbufp_collect(spdid_t spdid, int size, long cbid)
{
	long *buf;
	int off = 0;
	struct cbufp_info *cbi;
	struct cbufp_comp_info *cci;
	int ret = -EINVAL;

	printl("cbufp_collect\n");

	buf = cbuf2buf(cbid, PAGE_SIZE);
	if (!buf) return -1;

	CBUFP_TAKE();
	cci = cbufp_comp_info_get(spdid);
	if (!cci) {
		CBUFP_RELEASE();
		return -ENOMEM;
	}

	/* Go through all cbufs we own, and save all of them that have
	 * no current references to them. */
	cbi = cci->cbufs.c;
	do {
		if (!cbi) break;
		if (!cbufp_referenced(cbi)) {
			cbufp_references_clear(cbi);
			buf[off++] = cbi->cbid;
			if (off == PAGE_SIZE/sizeof(int)) break;
		}
		cbi = FIRST_LIST(cbi, next, prev);
	} while (cbi != cci->cbufs.c);
	CBUFP_RELEASE();
	/* nothing collected...create a new one! */
	/* TODO: only allocate when we should, and sleep otherwise */
	/* if (off == 0) { */
	/* 	int r = cbufp_create(spdid, size, 0); */
	/* 	if (r) buf[off++] = r; */
	/* } */
	ret = off;
	return ret;
}

int
cbufp_delete(spdid_t spdid, int cbid)
{
	struct cbufp_comp_info *cci;
	struct cbufp_info *cbi;
	struct cbuf_meta *meta;
	int ret = -EINVAL;

	printl("cbufp_delete\n");
	assert(0);
	CBUFP_TAKE();
	cci = cbufp_comp_info_get(spdid);
	if (!cci) goto done;
	cbi = cmap_lookup(&cbufs, cbid);
	if (!cbi) goto done;
	
	meta = cbi->owner.m;
	if (meta) {
		/* TODO: check if free in all components, unmap, etc... */
		memset(meta, 0, sizeof(struct cbuf_meta));
	}
	cbufp_free_unmap(spdid, cbi);

	ret = 0;
done:
	CBUFP_RELEASE();
	return ret;
}

int
cbufp_retrieve(spdid_t spdid, int cbid, int len)
{
	struct cbufp_comp_info *cci;
	struct cbufp_info *cbi;
	struct cbuf_meta *meta;
	struct cbufp_maps *map;
	vaddr_t dest;
	void *page;
	int ret = -1;

	printl("cbufp_retrieve\n");

	CBUFP_TAKE();
	cci = cbufp_comp_info_get(spdid);
	if (!cci) goto done;
	cbi = cmap_lookup(&cbufs, cbid);
	if (!cbi) goto done;
	/* shouldn't cbuf2buf your own buffer! */
	if (cbi->owner.spdid == spdid) goto done;
	meta = cbufp_meta_lookup(cci, cbid);
	if (!meta) goto done;

	map        = malloc(sizeof(struct cbufp_maps));
	if (!map) goto done;
	dest = (vaddr_t)valloc_alloc(cos_spd_id(), spdid, 1);
	if (!dest) goto free;

	map->spdid = spdid;
	map->m     = meta;
	map->addr  = dest;
	INIT_LIST(map, next, prev);
	ADD_LIST(&cbi->owner, map, next, prev);

	page = cbi->mem;
	assert(page);
	if (dest != (mman_alias_page(cos_spd_id(), (vaddr_t)page, spdid, dest))) {
		assert(0);
		valloc_free(cos_spd_id(), spdid, (void *)dest, 1);
	}

	meta->nfo.c.flags |= CBUFM_TOUCHED;
	meta->nfo.c.ptr    = map->addr >> PAGE_ORDER;
	meta->sz           = 4096;
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
	struct cbuf_meta *meta;
	void *p;
	vaddr_t dest, ret = 0;

	printl("cbufp_register\n");
	CBUFP_TAKE();
	cci = cbufp_comp_info_get(spdid);
	if (!cci) goto done;
	cmr = cbufp_meta_lookup_cmr(cci, cbid);
	if (cmr) {
		ret = cmr->dest;
		goto done;
	}

	/* Create the mapping into the client */
	if (cbufp_alloc_map(spdid, &dest, &p, PAGE_SIZE)) goto done;
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

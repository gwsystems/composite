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
#include <cos_alloc.h>
#include <cmap.h>
#include <cos_list.h>

#include <stkmgr.h>

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

struct cbuf_comp_info {
	spdid_t spdid;
	struct cbuf_shared_page *csp;
	vaddr_t dest_csp;
	int nbin;
	struct cbuf_bin cbufs[CBUF_MAX_NSZ];
	struct cbuf_meta_range *cbuf_metas;
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

struct cbuf_info *cbi_head;

static struct cbuf_meta_range *
cbuf_meta_lookup_cmr(struct cbuf_comp_info *comp, u32_t cbid)
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
	void *p;
	memset(cci, 0, sizeof(struct cbuf_comp_info));
	cci->spdid = spdid;
	cvect_add(&components, cci, spdid);
}

static struct cbuf_comp_info *
cbuf_comp_info_get(spdid_t spdid)
{
	struct cbuf_comp_info *cci;

	cci = cvect_lookup(&components, spdid);
	if (!cci) {
		cci = malloc(sizeof(struct cbuf_comp_info));
		if (!cci) return NULL;
		cbuf_comp_info_init(spdid, cci);
	}
	return cci;
}

static struct cbuf_bin *
cbuf_comp_info_bin_get_by_index(struct cbuf_comp_info *cci, int index)
{
	int i;

	assert(index >= 0);
	assert(index < cci->nbin);

	return &cci->cbufs[i];
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
		if (d != (mman_alias_page(cos_spd_id(), ((vaddr_t)page) + off, spdid, d, flags))) {
			printd("couldn't alias(%ld, %lx, %d, %x, %d)\n", cos_spd_id(), ((vaddr_t)page) + off, (int) spdid, (unsigned int) d, flags);
			assert(0); /* TODO: roll back the aliases, etc... */
		}
	}
	return 0;
}

static int
cbuf_alloc_map(spdid_t spdid, vaddr_t *daddr, void **page, int size, vaddr_t exact_dest)
{
	void *p;
	vaddr_t dest = 0;
	int ret = 0;

	assert(size == (int)round_to_page(size));
	p = page_alloc(size/PAGE_SIZE);
	if (!p) goto done;
	memset(p, 0, size);

	if (exact_dest) {
		printd("mapping to exact dest\n");
		int tmp = valloc_alloc_at(cos_spd_id(), spdid, (void *) exact_dest, size/PAGE_SIZE);
		assert(!tmp);
		dest = exact_dest;
	}
	else {
		dest = (vaddr_t)valloc_alloc(cos_spd_id(), spdid, size/PAGE_SIZE);
	}
	if (!dest) goto free;

if (spdid == 12) printc(">>>1 about to call cbuf map to spd %d daddr %lx\n", spdid, dest);
	if (!cbuf_map(spdid, dest, p, size, MAPPING_RW)) goto done;

free:
	if (dest) valloc_free(cos_spd_id(), spdid, (void *)dest, 1);
	dest = 0;
	page_free(p, size/PAGE_SIZE);
	p = 0;
	ret = -1;
done:
	*page  = p;
	*daddr = dest;
	return ret;
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
			if (CBUF_REFCNT(meta)) return 1;
			sent  += meta->snd_rcv.nsent;
			recvd += meta->snd_rcv.nrecvd;
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
			meta->snd_rcv.nsent = meta->snd_rcv.nrecvd = 0;
		}
		m = FIRST_LIST(m, next, prev);
	} while (m != &cbi->owner);

	return;
}

static void
cbuf_free_unmap(spdid_t spdid, struct cbuf_info *cbi)
{
	struct cbuf_maps *m = &cbi->owner;
	struct cbuf_bin *bin;
	struct cbuf_meta *in_free;
	void *ptr = cbi->mem;
	int off;
	struct cbuf_comp_info *cci;

	if (cbuf_referenced(cbi)) return;
	cbuf_references_clear(cbi);
	in_free = m->m->next;
	do {
		assert(m->m);
		assert(!CBUF_REFCNT(m->m));
		/* TODO: fix race here with atomic instruction */
		memset(m->m, 0, sizeof(struct cbuf_meta));

		m = FIRST_LIST(m, next, prev);
	} while (m != &cbi->owner);
	/*this cbuf is in freelist, set it as inconsistent*/
	if (in_free) CBUF_FLAG_ADD(cbi->owner.m, CBUF_INCONSISTENT);
	/* Unmap all of the pages from the clients */
	for (off = 0 ; off < cbi->size ; off += PAGE_SIZE) {
		mman_revoke_page(cos_spd_id(), (vaddr_t)ptr + off, 0);
	}

	/* 
	 * Deallocate the virtual address in the client, and cleanup
	 * the memory in this component
	 */
	m = FIRST_LIST(&cbi->owner, next, prev);
	while (m != &cbi->owner) {
		struct cbuf_maps *next;

		next = FIRST_LIST(m, next, prev);
		REM_LIST(m, next, prev);
		valloc_free(cos_spd_id(), m->spdid, (void*)m->addr, cbi->size/PAGE_SIZE);
		free(m);
		m = next;
	}
	valloc_free(cos_spd_id(), m->spdid, (void*)m->addr, cbi->size/PAGE_SIZE);

	/* deallocate/unlink our data-structures */
	page_free(ptr, cbi->size/PAGE_SIZE);
	cmap_del(&cbufs, cbi->cbid);
	cci = cbuf_comp_info_get(spdid);
	if (unlikely(!cci)) return ;
	bin = cbuf_comp_info_bin_get(cci, cbi->size);
	if (EMPTY_LIST(cbi, next, prev)) {
		bin->c = NULL;
		cci->nbin--;
	}
	else {
		if (bin->c == cbi) bin->c = cbi->next;
		REM_LIST(cbi, next, prev);
	}
	free(cbi);
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
			CBUF_FLAG_ADD(cbi->owner.m, CBUF_RELINQ);
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
			CBUF_FLAG_REM(cbi->owner.m, CBUF_RELINQ);
			cbi = FIRST_LIST(cbi, next, prev);
		} while (cbi != bin->c);
	}
}

static inline int
__cbuf_create(spdid_t spdid, int size, long cbid, vaddr_t dest)
{
	struct cbuf_comp_info *cci;
	struct cbuf_info *cbi;
	struct cbuf_meta *meta;
	int ret = 0;

	if (unlikely(cbid < 0)) return 0;
	
	cci = cbuf_comp_info_get(spdid);
	if (!cci) goto done;

	/* 
	 * Client wants to allocate a new cbuf, but the meta might not
	 * be mapped in.
	 */
	if (!cbid) {
		//printd("cb create !cbid\n");
		struct cbuf_bin *bin;

 		cbi = malloc(sizeof(struct cbuf_info));
		if (!cbi) goto done;

		/* Allocate and map in the cbuf. */
		cbid = cmap_add(&cbufs, cbi);
		printc("creating cbuf id %u\n", cbid);
		cbi->cbid        = cbid;
		size             = round_up_to_page(size);
		cbi->size        = size;
		cbi->owner.m     = NULL;
		cbi->owner.spdid = spdid;
		INIT_LIST(&cbi->owner, next, prev);
		INIT_LIST(cbi, next, prev);
	
		if (cbid == 28) printc("We are looking at cbid 28\n");
		if (cbid == 88) printc("We are looking at cbid 88\n");
		if (cbid == 1) printc("We are looking at cbid 1\n");

		if (cbi_head) {
			struct cbuf_info *cbi_new = (struct cbuf_info *) malloc(sizeof(struct cbuf_info));
			cbi_new->cbid = cbid;
			printc("adding cbuf id %u\n", cbid);

			ADD_END_LIST(cbi_head, cbi_new, next, prev);
		}
		else {
			cbi_head = (struct cbuf_info *) malloc(sizeof(struct cbuf_info));
			cbi_head->cbid = cbid;
			printc("adding cbuf id %u\n", cbid);
			
			INIT_LIST(cbi_head, next, prev);
		}

		bin = cbuf_comp_info_bin_get(cci, size);
		if (!bin) bin = cbuf_comp_info_bin_add(cci, size);
		if (!bin) goto free;

		if (cbuf_alloc_map(spdid, &(cbi->owner.addr), (void**)&(cbi->mem), size, dest)) goto free;
		if (bin->c) ADD_LIST(bin->c, cbi, next, prev);
		else        bin->c = cbi;
	} 
	/* If the client has a cbid, then make sure we agree! */
	else {
		//printd("cb create else\n");
		cbi = cmap_lookup(&cbufs, cbid);
		if (!cbi) goto done;
		if (cbi->owner.spdid != spdid) goto done;
	}
	meta = cbuf_meta_lookup(cci, cbid);
	
	//if (meta) { printd("cb create actually we did find a meta so what now?\n"); }
	/* We need to map in the meta for this cbid.  Tell the client. */
	if (!meta) {
		//printd("cb create !meta\n");
		ret = cbid * -1;
		goto done;
	}
	cbi->owner.m = meta;

	/* 
	 * Now we know we have a cbid, a backing structure for it, a
	 * component structure, and the meta mapped in for the cbuf.
	 * Update the meta with the correct addresses and flags!
	 */
	//printd("cb create final stuff\n");
	memset(meta, 0, 1); // sizeof(struct cbuf_meta));
	CBUF_FLAG_ADD(meta, CBUF_OWNER);
	CBUF_PTR_SET(meta, cbi->owner.addr);
	meta->sz = cbi->size >> PAGE_ORDER;
	meta->cbid_tag.cbid = cbid;
	assert(CBUF_REFCNT(meta) < CBUF_REFCNT_MAX);
	CBUF_REFCNT_ATOMIC_INC(meta);
	ret = cbid;

	//printd("finishing cbuf_create (presumably successfully?)\n");
done:
	return ret;

free:
	cmap_del(&cbufs, cbid);
	free(cbi);
	goto done;
}

int
cbuf_create(spdid_t spdid, int size, long cbid)
{
	int ret = 0;
	printl("cbuf_create\n");
	CBUF_TAKE();
	ret = __cbuf_create(spdid, size, cbid, 0);
	CBUF_RELEASE();
	return ret;
}

int
__map_and_meta(spdid_t spdid, int cbid, int size)
{
	struct cbuf_comp_info *cci;
	struct cbuf_info *cbi;
	struct cbuf_maps *map;
	vaddr_t dest;
	void *page;
	int ret = -EINVAL;

	printc("Entering cute lil' helper method with spdid %d, cbid %d, and size %d\n", spdid, cbid, size);
	
	cci        = cbuf_comp_info_get(spdid);
	if (!cci) {printd("no cci\n"); goto done; }
	cbi        = cmap_lookup(&cbufs, cbid);
	if (!cbi) {printd("no cbi\n"); goto done; }
	/* shouldn't cbuf2buf your own buffer! */
	if (cbi->owner.spdid == spdid) {printd("owner\n"); goto done;}

	map        = malloc(sizeof(struct cbuf_maps));
	if (!map) {printd("no map\n"); ERR_THROW(-ENOMEM, done); }
	if (size > cbi->size) {printd("too big\n"); goto done; }
	assert((int)round_to_page(cbi->size) == cbi->size);
	size       = cbi->size;
	dest       = (vaddr_t)valloc_alloc(cos_spd_id(), spdid, size/PAGE_SIZE);
	if (!dest) {printd("no valloc\n"); goto free; }

	map->spdid = spdid;
	map->m     = NULL;
	map->addr  = dest;
	INIT_LIST(map, next, prev);
	ADD_LIST(&cbi->owner, map, next, prev);

	page = cbi->mem;
	assert(page);
if (spdid == 12) printc(">>>3 about to call cbuf map to spd %d daddr %lx\n", spdid, dest);
	printd("cbuf_map(%d, %lx, %x, %d, %d)\n", spdid, dest, (unsigned int) page, size, MAPPING_RW);
	if (cbuf_map(spdid, dest, page, size, MAPPING_RW)) {
		printd("cbuf_map failed\n");
		valloc_free(cos_spd_id(), spdid, (void *)dest, 1);
	}
	ret = 0;
done:
	return ret;
free:
	free(map);
	goto done;
}


// REALLY feel like we don't need cbid and id here.
// Can replace id with cbid - they actually ARE the same parameter.
// Cbuf_unpack just makes sure it is an individual cbuf not an aggregate.
// It then dumps cbid and starts using id
vaddr_t
__cbuf_map_at(spdid_t s_spd, cbuf_t cbid, u32_t id, spdid_t d_spd, vaddr_t d_addr)
{
	struct cbuf_info *cbi;
	int flags;
	vaddr_t ret = (vaddr_t)NULL;

	if (d_spd == 12) printc(">>>4 mapping cbuf %u\n", cbid / 4);
	
	cbi = cmap_lookup(&cbufs, id);
	if (!cbi) printc(">>> no cbuf\n");
	assert(cbi);															// wait, first we assert this but then if it's not there we go to done, which is it?
	if (unlikely(!cbi)) goto done;
	assert(cbi->owner.spdid == s_spd);
	
	// the low-order bits of the d_addr are packed with the MAPPING flags (0/1)
	// and a flag (2) set if valloc should not be used.
	flags = d_addr & 0x3;
	d_addr &= ~0x3;
	if (!(flags & 2) && valloc_alloc_at(s_spd, d_spd, (void*)d_addr, cbi->size/PAGE_SIZE)) goto done;
if (d_spd == 12) printc(">>>2 about to call cbuf map to spd %d daddr %lx\n", d_spd, d_addr);
	if (cbuf_map(d_spd, d_addr, cbi->mem, cbi->size, flags & (MAPPING_READ|MAPPING_RW))) goto free;
	ret = d_addr;
	/* do not add d_spd to the meta list because the cbuf is not
	 * accessible directly. The s_spd must maintain the necessary info
	 * about the cbuf and its mapping in d_spd. */
	
	
	// Does this go here??? I'm just guessing
	__map_and_meta(d_spd, id, cbi->size);
done:
	if (d_spd == 12) printc(">>>ret: %x\n", ret);
	return ret;
free:
	if (!(flags & 2)) valloc_free(s_spd, d_spd, (void*)d_addr, cbi->size);
	goto done;
}

/*
 * returns address this is mapped to.
 * Why? Isn't this address provided? It CAN just return 0 on success to be more like cbuf_map
 */
vaddr_t
cbuf_map_at(spdid_t s_spd, cbuf_t cbid, spdid_t d_spd, vaddr_t d_addr)
{
	vaddr_t ret = (vaddr_t)NULL;
	u32_t id;
	
	cbuf_unpack(cbid, &id);
	CBUF_TAKE();
	ret = __cbuf_map_at(s_spd, cbid, id, d_spd, d_addr);
	CBUF_RELEASE();
	return ret;
}

int
cbuf_unmap_at(spdid_t s_spd, cbuf_t cbid, spdid_t d_spd, vaddr_t d_addr)
{
	struct cbuf_info *cbi;
	int off;
	int ret = 0;
	u32_t id;
	int err = 0;
	
	cbuf_unpack(cbid, &id);

	assert(d_addr);
	CBUF_TAKE();
	cbi = cmap_lookup(&cbufs, id);
	if (unlikely(!cbi)) ERR_THROW(-EINVAL, done);
	if (unlikely(cbi->owner.spdid != s_spd)) ERR_THROW(-EPERM, done);
	assert(cbi->size == (int)round_to_page(cbi->size));
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
	f_cci = cbuf_comp_info_get(f_spd);
	if (!o_cci) goto done;
	if (!f_cci) goto done;
	o_cmr = cbuf_meta_lookup_cmr(o_cci, o_cbid);
	if (!o_cmr) goto done;
	f_cmr = cbuf_meta_lookup_cmr(f_cci, f_cbid);
	if (f_cmr) ERR_THROW(f_cmr->dest, done);
	
	/* Create the mapping into the client */
	if (cbuf_alloc_map(f_spd, &dest, &p, PAGE_SIZE, o_cmr->dest)) goto done;
	assert((u32_t)p == round_to_page(p));
	f_cmr = cbuf_meta_add(f_cci, f_cbid, p, dest);
	assert(f_cmr);
	ret = f_cmr->dest;
	
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
	assert(cci);
	int i;

	for (i = 0; i < cci->nbin; i++) {
		struct cbuf_info *cb_nfo = cci->cbufs[i].c;
		assert(cb_nfo);
		struct cbuf_info *start = cb_nfo;
		do {
			printc("cci for spdid %d, cbuf info id %d starting at %x (in cbuf_mgr), %lx (in comp) size %x\n", cci->spdid, cb_nfo->cbid, (unsigned int) cb_nfo->mem, cb_nfo->owner.addr, cb_nfo->size);
			cb_nfo = cb_nfo->next;
		} while (cb_nfo != start);
	}

	// technique 2
	printc("Now iterating over global cbuf list\n");
	// iterate over cbi_head until done
	struct cbuf_info *current = cbi_head;
	do {
		// The cbuf_info list we made is actually crap because it copies the pointers so if the structure is updated it is no longer valid (how to fix???)
		// instead, re-get the cbi from the cbuf_id which we assume is constant
		struct cbuf_info *cbi = cmap_lookup(&cbufs, current->cbid);

		printc("current cbi #%2u with size %x, memory start %x, and maps ", cbi->cbid, cbi->size, cbi->mem);
		struct cbuf_maps *m = &cbi->owner;

		do {
			printc("[%d:%x] ", m->spdid, m->addr);
			//printc("DEBUG: (%x) (<-%x) (->%x) ", m, m->prev, m->next);

			m = FIRST_LIST(m, next, prev);
			//printc("DEBUG: (%x)", m);
		} while (m != &cbi->owner);

		printc("\n");

		//printc("cbi->owner: %x<-%x->%x\n", cbi->owner.prev, &cbi->owner, cbi->owner.next);

		current = FIRST_LIST(current, next, prev);
	} while (current != cbi_head);
}

void test(void) {
	spdid_t spd_C = 6;
	spdid_t spd_O = 12;
	spdid_t spd_F = 14;

	vaddr_t dest = (vaddr_t) valloc_alloc(cos_spd_id(), spd_C, 1);

	vaddr_t dest_O = mman_alias_page(cos_spd_id(), dest, spd_O, dest, MAPPING_RW);
	vaddr_t dest_F = mman_alias_page(cos_spd_id(), dest, spd_F, dest, MAPPING_READ);

	printd("a\n");
	memset(&dest, 0, PAGE_SIZE);
	printd("b\n");
	memset(&dest_O, 0, PAGE_SIZE);
	printd("c\n");
	memset(&dest_F, 0, PAGE_SIZE);
	printd("d\n");
}

// feel like this method needs to be renamed but... should make it work first
static int
__cbuf_copy_cci(spdid_t o_spd, struct cbuf_comp_info *src, spdid_t f_spd, struct cbuf_comp_info *dst)
{
	//test();
	__get_nfo(src);

	int i;
	/* Should create a new shared page between cbuf_mgr and dst */
	dst->csp = src->csp;
	dst->dest_csp = src->dest_csp;
	//dst->nbin = 0; // something else probably inits this. Remove?
	for (i = 0; i < src->nbin; i++) {
		spdid_t spdid = f_spd;
		int size = src->cbufs[i].size;
		struct cbuf_info *cb_info = src->cbufs[i].c;
		struct cbuf_info *start = cb_info;
		
		do {
			printd("\n\nabout to try coping cbuf %d\n", cb_info->cbid);

			// find source cbuf_maps
			struct cbuf_maps *current = &cb_info->owner;
			struct cbuf_maps *src_map = NULL;
			vaddr_t dest;
			int cbid = 0;
			
			while (current->spdid != o_spd) current = current->next;
			src_map = current;
			dest = src_map->addr;

			/* create cbuf */
			cbid = __cbuf_create(spdid, size, cbid * -1, dest);
			printd("cbid after first create is %d. Should be negative because there is no map\n", cbid);
			if (cbid < 0) { // only do these steps if initial create failed because of lack of mep
				if (!__cbuf_register_map_at(o_spd, f_spd, cb_info->cbid, cbid * -1)) { printd("cbuf_register failed\n"); goto done; }
				cbid = __cbuf_create(spdid, size, cbid * -1, dest);
				printd("cbid after second create is %d. Should be good now\n", cbid);
			}

			if (cbid <= 0) { 
				printd("cbi didn't agree\n");
				goto done;
				struct cbuf_info *cbi = cmap_lookup(&cbufs, cb_info->cbid);
				cbi->owner.spdid = f_spd;
			}
			assert(cbid);
		
			printd("copied cbuf id is %d, compared to src cbuf %u. These probably won't match, which is good because it gives us something to do next.\n", cbid, cb_info->cbid);

			/* copy into cbuf */
			char *src_mem = cb_info->mem;
			assert(src_mem);
			
			struct cbuf_bin *dst_bin = &dst->cbufs[i];
			assert(dst_bin);
			struct cbuf_info *dst_info = dst_bin->c;
			assert(dst_info);
			while (dst_info->cbid != (u32_t) cbid) dst_info = dst_info->next;
			char *dst_mem = dst_info->mem;
			assert(dst_mem);

			vaddr_t addr_in_spd = dst_info->owner.addr;
			printc("in F, dest is %lx. dst_info's size is %x\n", addr_in_spd, (unsigned int) dst_info->size);

			printc("d %x s %x l %x\n", (unsigned int) dst_mem, (unsigned int) src_mem, size);
			void *ret = memcpy(dst_mem, src_mem, size);
			assert(ret == dst_mem);

			cb_info = cb_info->next;
			dst_info = dst_info->next;
			assert(dst_info);
		} while (cb_info != start);
	}
	
	assert(dst->cbuf_metas);
	for (i = 0; i < dst->nbin; i++) {
		printd("Doing a sanity check for i = %d\n", i);
		spdid_t spdid = f_spd;
		struct cbuf_meta *meta = cbuf_meta_lookup(dst, src->cbufs[i].c->cbid);
		if (meta == NULL) {
			printd("something has already gone wrong\n");
		}
		else {
			printd("yaaaay *clap-clap*\n");
		}

		struct cbuf_maps *current = &dst->cbufs[i].c->owner;
		struct cbuf_maps *start = current;
		while (current->spdid != f_spd) {
			current = current->next;
			if (current == start) BUG();
		}
		printd("cbuf map for F now has spd %d and addr %x\n", current->spdid, (unsigned int) current->addr);

		printd("getting dst cci info\n");
		__get_nfo(dst);
	}

done:
	return 0; // wow this is random. See what it returned before, do something better.
}

int cbuf_fork_spd(spdid_t spd, spdid_t o_spd, spdid_t f_spd)
{
	struct cbuf_comp_info *o_cci, *f_cci;
	int ret = 0;

	CBUF_TAKE();
	o_cci = cbuf_comp_info_get(o_spd);
	if (unlikely(!o_cci)) goto done;
	f_cci = cbuf_comp_info_get(f_spd);
	if (unlikely(!f_cci)) goto done;
	ret = __cbuf_copy_cci(o_spd, o_cci, f_spd, f_cci);
	printd("Finished copy cci\n");

done:
	CBUF_RELEASE();
	return ret;
}

void cos_fix_spdid_metadata(spdid_t o_spd, spdid_t f_spd)
{
	int r;

	printd("cbuf: cos_fix_spdid_metadata for %d -> %d\n", o_spd, f_spd);
	
	r = cbuf_fork_spd(cos_spd_id(), o_spd, f_spd);
	if (r) printd("Error (%d) in cbuf_fork_spd\n", r);
	/* TODO: invoke valloc to get transitive fixup? */
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
	if (cbuf_alloc_map(spdid, &cci->dest_csp, (void**)&cci->csp, PAGE_SIZE, 0)) goto done;
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
		if (!CBUF_IS_IN_FREELiST(cbi->owner.m) && !cbuf_referenced(cbi)) {
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

/* 
 * Called by cbuf_deref.
 */
int
cbuf_delete(spdid_t spdid, int cbid)
{
	struct cbuf_comp_info *cci;
	struct cbuf_info *cbi;
	int ret = -EINVAL;

	printl("cbuf_delete\n");
	assert(0);
	CBUF_TAKE();
	cci = cbuf_comp_info_get(spdid);
	if (!cci) goto done;
	cbi = cmap_lookup(&cbufs, cbid);
	if (!cbi) goto done;
	
	cbuf_free_unmap(spdid, cbi);
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
	int ret = -EINVAL;

	printl("cbuf_retrieve\n");

	CBUF_TAKE();

	cci        = cbuf_comp_info_get(spdid);
	if (!cci) {printd("no cci\n"); goto done; }
	cbi        = cmap_lookup(&cbufs, cbid);
	if (!cbi) {printd("no cbi\n"); goto done; }
	/* shouldn't cbuf2buf your own buffer! */
	if (cbi->owner.spdid == spdid) {printd("owner\n"); goto done;}
	meta       = cbuf_meta_lookup(cci, cbid);
	if (!meta) {printd("no meta\n"); goto done; }

	map        = malloc(sizeof(struct cbuf_maps));
	if (!map) {printd("no map\n"); ERR_THROW(-ENOMEM, done); }
	if (size > cbi->size) {printd("too big\n"); goto done; }
	assert((int)round_to_page(cbi->size) == cbi->size);
	size       = cbi->size;
	dest       = (vaddr_t)valloc_alloc(cos_spd_id(), spdid, size/PAGE_SIZE);
	if (!dest) {printd("no valloc\n"); goto free; }

	map->spdid = spdid;
	map->m     = meta;
	map->addr  = dest;
	INIT_LIST(map, next, prev);
	ADD_LIST(&cbi->owner, map, next, prev);

	// iterate to test things thing
	printc("ITERATING DURING CB RETRIEVE\n");
	struct cbuf_maps *m = &cbi->owner;
	do {
		printc("%x->", m);
		m = FIRST_LIST(m, next, prev);
	} while (m != &cbi->owner);
	printc("\n");

	page = cbi->mem;
	assert(page);
if (spdid == 12) printc(">>>3 about to call cbuf map to spd %d daddr %lx\n", spdid, dest);
	printd("cbuf_map(%d, %lx, %x, %d, %d)\n", spdid, dest, (unsigned int) page, size, MAPPING_RW);
	if (cbuf_map(spdid, dest, page, size, MAPPING_RW)) {
		printd("cbuf_map failed\n");
		valloc_free(cos_spd_id(), spdid, (void *)dest, 1);
	}
	memset(meta, 0, sizeof(struct cbuf_meta));
	CBUF_PTR_SET(meta, map->addr);
	meta->sz        = cbi->size >> PAGE_ORDER;
	meta->cbid_tag.cbid = cbid;
	ret             = 0;

done:
	CBUF_RELEASE();
	return ret;
free:
	free(map);
	goto done;
	return ret;
}

vaddr_t
__cbuf_register(spdid_t spdid, long cbid)
{
	struct cbuf_comp_info  *cci;
	struct cbuf_meta_range *cmr;
	void *p;
	vaddr_t dest, ret = 0;

	printl("__cbuf_register\n");
	printd("__cbuf_register: spdid %d cbid %ld\n", spdid, cbid);
	cci = cbuf_comp_info_get(spdid);
	if (!cci) goto done;
	cmr = cbuf_meta_lookup_cmr(cci, cbid);
	if (cmr) ERR_THROW(cmr->dest, done);

	/* Create the mapping into the client */
	if (cbuf_alloc_map(spdid, &dest, &p, PAGE_SIZE, 0)) goto done;
	assert((u32_t)p == round_to_page(p));
	cmr = cbuf_meta_add(cci, cbid, p, dest);
	assert(cmr);
	ret = cmr->dest;
	
done:
	return ret;
}

vaddr_t
cbuf_register(spdid_t spdid, long cbid)
{
	vaddr_t ret = 0; 
	CBUF_TAKE();
	ret = __cbuf_register(spdid, cbid);
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

	if (cbuf_alloc_map(d_spdid, &d_addr, (void**)&p, PAGE_SIZE, 0)) goto done;
	ret = (void*)D_COS_STK_ADDR(d_addr);

done:
	CBUF_RELEASE();
	return ret;
}


void
cos_init(void)
{
	long cbid;
	CBUF_LOCK_INIT();
	cmap_init_static(&cbufs);
	cbid = cmap_add(&cbufs, NULL);
}

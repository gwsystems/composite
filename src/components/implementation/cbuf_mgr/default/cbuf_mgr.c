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
	printd("cml: cbid %d low_id %d cbid - lowid %d\n", cbid, cmr->low_id, cbid - cmr->low_id);
	return &cmr->m[cbid - cmr->low_id];
}

/*
 * What does this do? Initializes a meta range, adds the provided meta to it.
 */
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
cbuf_alloc_map(spdid_t spdid, vaddr_t *daddr, void **page, int size)
{
	void *p;
	vaddr_t dest = 0;
	int ret = 0;

	assert(size == (int)round_to_page(size));
	p = page_alloc(size/PAGE_SIZE);
	if (!p) goto done;
	memset(p, 0, size);

	dest = (vaddr_t)valloc_alloc(cos_spd_id(), spdid, size/PAGE_SIZE);
	if (!dest) goto free;

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
__cbuf_create(spdid_t spdid, int size, long cbid)
{
	struct cbuf_comp_info *cci;
	struct cbuf_info *cbi;
	struct cbuf_meta *meta;
	int ret = 0;
	
	if (unlikely(cbid < 0)) return 0;
	
	cci = cbuf_comp_info_get(spdid);
	if (!cci) goto done;

	printd("__cbuf_create spdid %d, cbid %d\n", spdid, cbid);

	/* 
	 * Client wants to allocate a new cbuf, but the meta might not
	 * be mapped in.
	 */
	if (!cbid) {
		printd("create: !cbid\n");
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

		if (cbuf_alloc_map(spdid, &(cbi->owner.addr), (void**)&(cbi->mem), size)) goto free;
		if (bin->c) ADD_LIST(bin->c, cbi, next, prev);
		else        bin->c = cbi;
	} 
	/* If the client has a cbid, then make sure we agree! */
	else {
		printd("create: else\n");
		cbi = cmap_lookup(&cbufs, cbid);
		printd("create: cmap_lookup succeeded\n");
		if (!cbi) goto done;
		printd("create: cbi found\n");
		if (cbi->owner.spdid != spdid) goto done;
	}
	printd("create: looking up meta in spd %d with cbid %d\n", cci->spdid, cbid);
	meta = cbuf_meta_lookup(cci, cbid);
	printd("create: cbuf_meta_lookup succeeded with cbuf_meta address %x\n", meta);
	/* We need to map in the meta for this cbid.  Tell the client. */
	if (!meta) {
		printd("create: !meta\n");
		ret = cbid * -1;
		goto done;
	}
	cbi->owner.m = meta;

	/* 
	 * Now we know we have a cbid, a backing structure for it, a
	 * component structure, and the meta mapped in for the cbuf.
	 * Update the meta with the correct addresses and flags!
	 */
	printd("create: starting memset\n");
	memset(meta, 0, 1); // sizeof(struct cbuf_meta));
	printd("create: finished mem_set\n");
	CBUF_FLAG_ADD(meta, CBUF_OWNER);
	CBUF_PTR_SET(meta, cbi->owner.addr);
	printd("cb_create: 9\n");
	meta->sz = cbi->size >> PAGE_ORDER;
	meta->cbid_tag.cbid = cbid;
	printd("cb_create: 10\n");
	assert(CBUF_REFCNT(meta) < CBUF_REFCNT_MAX);
	CBUF_REFCNT_ATOMIC_INC(meta);
	ret = cbid;
	printd("cb_create: 11\n");
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
	ret = __cbuf_create(spdid, size, cbid);
	CBUF_RELEASE();
	return ret;
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
	vaddr_t ret;
	
	cbi = cmap_lookup(&cbufs, id);
	assert(cbi);
	if (unlikely(!cbi)) goto done;
	assert(cbi->owner.spdid == s_spd);
	// the low-order bits of the d_addr are packed with the MAPPING flags (0/1)
	// and a flag (2) set if valloc should not be used.
	flags = d_addr & 0x3;
	d_addr &= ~0x3;
	if (!(flags & 2) && valloc_alloc_at(s_spd, d_spd, (void*)d_addr, cbi->size/PAGE_SIZE)) goto done;
	if (cbuf_map(d_spd, d_addr, cbi->mem, cbi->size, flags & (MAPPING_READ|MAPPING_RW))) goto free;
	ret = d_addr;
	/* do not add d_spd to the meta list because the cbuf is not
	 * accessible directly. The s_spd must maintain the necessary info
	 * about the cbuf and its mapping in d_spd. */
done:
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
	dest = o_cmr->dest;
	f_cmr = cbuf_meta_lookup_cmr(f_cci, f_cbid);
	if (f_cmr) ERR_THROW(f_cmr->dest, done);
	/* Create the mapping into the client */

	printd("o_spd %d o_cbid %d f_spd %d f_cbid %d \n", o_spd, o_cbid, f_spd, f_cbid);
	if (!__cbuf_map_at(o_spd, o_cbid, o_cbid, f_spd, dest)) goto done;
	assert((u32_t)p == round_to_page(p));
	f_cmr = cbuf_meta_add(f_cci, f_cbid, p, dest);
	assert(f_cmr);
	printd("cbuf_register: did a meta_add\n");

	struct cbuf_meta *cm1 = cbuf_meta_lookup(o_cci, o_cbid);
	if (!cm1) goto done;
	printd("trying to do memset\n");
	memset(cm1, 0, sizeof(struct cbuf_meta));
	
	struct cbuf_meta *cm3 = cbuf_meta_lookup(f_cci, o_cbid);
	if (!cm3) goto done;
	printd("trying to do memset 3\n");
	memset(cm3, 0, sizeof(struct cbuf_meta));
	struct cbuf_meta *cm2 = cbuf_meta_lookup(f_cci, f_cbid);
	if (!cm2) goto done;
	printd("trying to do memset 2\n");
	memset(cm2, 0, sizeof(struct cbuf_meta));


	ret = f_cmr->dest;
done:
	return ret;
}

void
test(void)
{
	spdid_t spd_C = 6;
	spdid_t spd_O = 12;
	spdid_t spd_F = 14;
	vaddr_t dest = (vaddr_t)valloc_alloc(cos_spd_id(), spd_C, 1);
	
	vaddr_t dest_O = mman_alias_page(cos_spd_id(), dest, spd_O, dest, MAPPING_RW);
	vaddr_t dest_F = mman_alias_page(cos_spd_id(), dest, spd_F, dest, MAPPING_READ);
	
	printd("a\n");
	memset(&dest, 0, PAGE_SIZE);
	printd("b\n");
	memset(&dest_O, 0, PAGE_SIZE); 
	printd("c\n");
	memset(&dest_F, 0, PAGE_SIZE);
	printd("c\n");
}

// feel like this method needs to be renamed but... should make it work first
static int
__cbuf_copy_cci(spdid_t o_spd, struct cbuf_comp_info *src, spdid_t f_spd, struct cbuf_comp_info *dst)
{
	test();
	
	int i;
	/* Should create a new shared page between cbuf_mgr and dst */
	dst->csp = src->csp;
	dst->dest_csp = src->dest_csp;
	/* probably shouldn't be copying all these? but should get
	 * to access them somehow. */
	dst->nbin = 0; // something else probably inits this. Remove?
	printd("src->nbin is %d\n", src->nbin);
	for (i = 0; i < src->nbin; i++) {
		printd("cbuf_copy_cci: taking care of O's cbuf %d, which has cbufid %d\n", i, src->cbufs[i].c->cbid);
		
		spdid_t spdid = f_spd;
		int size = src->cbufs[i].size;
		long cbid = 0;

		printd("calling cbuf_create\n");
		cbid = __cbuf_create(spdid, size, cbid * -1);
		if (cbid < 0 && !__cbuf_register_map_at(o_spd, f_spd, src->cbufs[i].c->cbid, cbid * -1)) { printd("cbuf_register failed\n"); goto done; }
		cbid = __cbuf_create(spdid, size, cbid * -1);
		if (cbid == 0) { 
			printd("cbi didn't agree\n");
			goto done;
			struct cbuf_info *cbi = cmap_lookup(&cbufs, src->cbufs[i].c->cbid);
			cbi->owner.spdid = f_spd;
		}

			
		assert(cbid);

		printd("copied cbuf id is %d, compared to src cbuf %d. These probably won't match, which is good because it gives us something to do next.\n", cbid, src->cbufs[i].c->cbid);

		///* copy into cbuf */
		//struct cbuf_meta *cm = cbuf_meta_lookup(src, cbid);
		//char *dst_mem_start = (char*)(CBUF_PTR(cm));
		//assert(dst_mem_start != NULL);
		//struct cbuf_bin *bin = cbuf_comp_info_bin_get(src, size);
		//assert(bin != NULL);
		//struct cbuf_info *cb_info = bin->c;
		//assert(cb_info != NULL);
		//struct cbuf_maps owner = cb_info->owner;
		//assert(&owner != NULL);
		//while (owner.spdid != spdid) {
		//	printd("a\n");
		//	if (owner.next == NULL) BUG();
		//	owner = *(owner.next);
		//}
		//char *src_mem_start = owner.addr;
		//printd("setting an addr\n");
		//assert(src_mem_start != NULL);
		//printc("d %x s %x l %d\n", dst_mem_start, src_mem_start, size);
		//void *ret = memcpy(dst_mem_start, src_mem_start, size);
		//assert(ret == dst_mem_start);
		//printd("cbuf_copy_cci: did memcpy and got a result we should really print here\n");

		/* map cbuf */
	}
	printd("following everything, dst->nbin is %d\n", dst->nbin);
	
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
	__cbuf_copy_cci(o_spd, o_cci, f_spd, f_cci);
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
	int ret = -EINVAL, off;

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

	page = cbi->mem;
	assert(page);
	printd("cbuf_map(%d, %x, %x, %d, %d)\n", spdid, (unsigned int) dest, (unsigned int) page, size, MAPPING_RW);
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
}

vaddr_t
__cbuf_register(spdid_t spdid, long cbid)
{
	struct cbuf_comp_info  *cci;
	struct cbuf_meta_range *cmr;
	void *p;
	vaddr_t dest, ret = 0;

	printl("__cbuf_register\n");
	printd("__cbuf_register: spdid %d cbid %d\n", spdid, cbid);
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
	return ret;
}

vaddr_t
cbuf_register(spdid_t spdid, long cbid)
{
	vaddr_t ret = 0; 
	printd("cbuf_register\n");
	CBUF_TAKE();
	ret = __cbuf_register(spdid, cbid);
	CBUF_RELEASE();

	printd("cbuf_register ret: %x\n", ret);
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

	if (cbuf_alloc_map(d_spdid, &d_addr, (void**)&p, PAGE_SIZE)) goto done;
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

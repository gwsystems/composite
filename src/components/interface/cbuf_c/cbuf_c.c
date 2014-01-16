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

#include <cos_component.h>
#include <print.h>
#include <cbuf.h>
#include <cbufp.h>
#include <cbuf_vect.h>
#include <cos_vect.h>
#include <cos_debug.h>
#include <cos_alloc.h>
#include <valloc.h>

#define CSLAB_ALLOC(sz)   alloc_page()
#define CSLAB_FREE(x, sz) free_page(x)
#include <cslab.h>
CSLAB_CREATE(desc, sizeof(struct cbuf_alloc_desc));

cos_lock_t cbuf_lock;
/* 
 * All of the structures that must be compiled (only once) into each
 * component that uses cbufs.
 *
 * I'd like to avoid having to make components initialize this data,
 * so try and use structures that by default are initialized as all
 * zero. 
 */
extern struct cos_component_information cos_comp_info;

CVECT_CREATE_STATIC(meta_cbuf);
CVECT_CREATE_STATIC(meta_cbufp);
CVECT_CREATE_STATIC(alloc_descs);
struct cbuf_alloc_desc cbuf_alloc_freelists = {.next = &cbuf_alloc_freelists, .prev = &cbuf_alloc_freelists, .addr = NULL};
struct cbuf_alloc_desc cbufp_alloc_freelists[CBUFP_MAX_NSZ];

/*** Manage the cbuf allocation descriptors and freelists  ***/

static struct cbuf_alloc_desc *
__cbuf_desc_alloc(int cbid, int size, void *addr, struct cbuf_meta *cm, int tmem)
{
	struct cbuf_alloc_desc *d;
	int idx = ((int)addr >> PAGE_ORDER);

	assert(addr && cm);
	assert(cm->nfo.c.ptr == idx);
	assert(__cbuf_alloc_lookup(idx) == NULL);
	assert((!tmem && !(cm->nfo.c.flags & CBUFM_TMEM)) ||
	       (tmem && cm->nfo.c.flags & CBUFM_TMEM));

	d = cslab_alloc_desc();
	if (!d) return NULL;

	d->cbid   = cbid;
	d->addr   = addr;
	d->length = size;
	d->meta   = cm;
	d->tmem   = tmem;
	INIT_LIST(d, next, prev);
	//ADD_LIST(&cbuf_alloc_freelists, d, next, prev);
	if (tmem) d->flhead = &cbuf_alloc_freelists;
	else      d->flhead = __cbufp_freelist_get(size);
	cvect_add(&alloc_descs, d, idx);

	return d;
}

/*
 * We got to this function because d and m aren't consistent.  This
 * can only happen because the manager removed a cbuf, 1) thus leaving
 * the meta pointer as NULL, and the freelist referring to no actual
 * cbuf, or 2) when another thread is given a new cbuf (via
 * cbuf_c_create) with the same cbid as the one referred to in the
 * freelist.  Either way, we want to simply remove the descriptor.
 */
void
__cbuf_desc_free(struct cbuf_alloc_desc *d)
{
	assert(d);
	assert(cvect_lookup(&alloc_descs, (unsigned long)d->addr >> PAGE_ORDER) == d);
	
	REM_LIST(d, next, prev);
	cvect_del(&alloc_descs, (unsigned long)d->addr >> PAGE_ORDER);
	cslab_free_desc(d);
}

/*** Slow paths for each cbuf operation ***/

/* 
 * Precondition: cbuf lock is taken.
 *
 * A component has tried to map a cbuf_t to a buffer, but that cbuf
 * isn't mapping into the component.  The component's cache of cbufs
 * had a miss.
 */
int 
__cbuf_2buf_miss(int cbid, int len, int tmem)
{
	struct cbuf_meta *mc;
	int ret;
	void *h;

	/* 
	 * FIXME: This can lead to a DOS where the client passes all
	 * possible cbids to be cbuf2bufed, which will allocate the
	 * entire 4M of meta vectors in this component.  Yuck.
	 * 
	 * Solution: the security mechanisms of the kernel (preventing
	 * cbids being passed to this component if they don't already
	 * belong to the client) should fix this.
	 */
	mc = cbuf_vect_lookup_addr(cbid_to_meta_idx(cbid), tmem);
	/* ...have to expand the cbuf_vect */
	if (unlikely(!mc)) {
		if (cbuf_vect_expand(tmem ? &meta_cbuf : &meta_cbufp, 
				     cbid_to_meta_idx(cbid), tmem)) BUG();
		mc = cbuf_vect_lookup_addr(cbid_to_meta_idx(cbid), tmem);
		assert(mc);
	}

	CBUF_RELEASE();
	if (tmem) ret = cbuf_c_retrieve(cos_spd_id(), cbid, len);
	else      ret = cbufp_retrieve(cos_spd_id(), cbid, len);
	CBUF_TAKE();
	if (unlikely(ret < 0                                   ||
		     mc->sz < (len >> PAGE_ORDER)              ||
		     (tmem && !(mc->nfo.c.flags & CBUFM_TMEM)) || 
		     (!tmem && mc->nfo.c.flags & CBUFM_TMEM))) {
		return -1;
	}
	assert(mc->nfo.c.ptr);
	if (tmem) mc->owner_nfo.thdid = 0;

	return 0;
}

static inline int
__cbufp_alloc_slow(int cbid, int size, int *len, int *error)
{
	int amnt = 0, i;
	cbuf_t cb;
	int *cbs;

	assert(cbid <= 0);
	if (cbid == 0) {
		struct cbuf_meta *cm;

		cbs    = cbuf_alloc(PAGE_SIZE, &cb);
		assert(cbs);
		cbs[0] = 0;
		/* Do a garbage collection */
		amnt = cbufp_collect(cos_spd_id(), size, cb);
		if (amnt < 0) {
			*error = 1;
			return -1;
		}

		CBUF_TAKE();
		cbid = cbs[0];
		/* own the cbuf we just collected */
		if (amnt > 0) {
			cm = cbuf_vect_lookup_addr(cbid_to_meta_idx(cbid), 0);
			assert(cm);
			/* (should be atomic) */
			/* cm->nfo.c.flags |= CBUFM_IN_USE | CBUFM_TOUCHED;  */
			cm->nfo.c.flags |= CBUFM_TOUCHED; 
			if(cm->nfo.c.refcnt == CBUFP_REFCNT_MAX)
				assert(0);
			cm->nfo.c.refcnt++;
		}
		/* ...add the rest back into freelists */
		for (i = 1 ; i < amnt ; i++) {
			struct cbuf_alloc_desc *d, *fl;
			struct cbuf_meta *meta;
			int idx = cbid_to_meta_idx(cbs[i]);
			u32_t page;
			void *data;

			assert(idx > 0);
			meta = cbuf_vect_lookup_addr(idx, 0);
			d    = __cbuf_alloc_lookup(meta->nfo.c.ptr);
			assert(d && d->cbid == cbs[i]);
			fl   = d->flhead;
			assert(fl);
			ADD_LIST(fl, d, next, prev);
		}
		CBUF_RELEASE();
		cbuf_free(cbs);
	}
	/* Nothing collected...allocate a new cbufp! */
	if (amnt == 0) {
		cbid = cbufp_create(cos_spd_id(), size, cbid*-1);
		if (cbid == 0) assert(0);
	} 
	/* TODO update correctly */
	*len = 1;

	return cbid;
}

/* 
 * Precondition: cbuf lock is taken.
 */
struct cbuf_alloc_desc *
__cbuf_alloc_slow(int size, int *len, int tmem)
{
	struct cbuf_alloc_desc *d_prev, *ret = NULL;
	struct cbuf_meta *cm;
	void *addr;
	int cbid;
	int cnt;

	cnt = cbid = 0;
	do {
		int error = 0;

		CBUF_RELEASE();
		if (tmem) {
			cbid = cbuf_c_create(cos_spd_id(), size, cbid*-1);
			*len = 0; /* tmem */
		} else {
			cbid = __cbufp_alloc_slow(cbid, size, len, &error);
			if (unlikely(error)) {
				CBUF_TAKE();
				ret = NULL;
				goto done;
			}
		}
		CBUF_TAKE();
		/* TODO: we will hold the lock in expand, which calls
		 * the manager...remove that */
		if (cbid < 0 && 
		    cbuf_vect_expand(tmem ? &meta_cbuf : &meta_cbufp, 
				     cbid_to_meta_idx(cbid*-1), tmem) < 0) goto done;
		/* though it's possible this is valid, it probably
		 * indicates an error */
		assert(cnt++ < 10); 
	} while (cbid < 0);
	assert(cbid);
	cm   = cbuf_vect_lookup_addr(cbid_to_meta_idx(cbid), tmem);
	assert(cm && cm->nfo.c.ptr);
	/* assert(cm && cm->nfo.c.flags & CBUFM_IN_USE); */
	assert(cm && cm->nfo.c.refcnt);
	assert(!tmem || cm->owner_nfo.thdid);
	addr = (void*)(cm->nfo.c.ptr << PAGE_ORDER);
	assert(addr);
	/* 
	 * See __cbuf_alloc and cbuf_slab_free.  It is possible that a
	 * slab descriptor will exist for a piece of cbuf memory
	 * _before_ it is allocated because it is actually from a
	 * previous cbuf.  If this is the case, then we should trash
	 * the old one and allocate a new one.
	 */
	/* TODO: check if this is correct. what if this cbuf is from
	 * the local cache and has been taken by another thd? */
	d_prev = __cbuf_alloc_lookup((u32_t)addr>>PAGE_ORDER);
	if (d_prev) __cbuf_desc_free(d_prev);
	ret    = __cbuf_desc_alloc(cbid, size, addr, cm, tmem);
done:   
	return ret;
}

CCTOR static void
cbuf_init(void)
{
	int i;

	lock_static_init(&cbuf_lock);
	for (i = 0 ; i < CBUFP_MAX_NSZ/2 ; i++) {
		cbufp_alloc_freelists[i].next = cbufp_alloc_freelists[i].prev = &cbufp_alloc_freelists[i];
		cbufp_alloc_freelists[i].length = PAGE_SIZE << i;
		cbufp_alloc_freelists[i].addr   = NULL;
	}
}

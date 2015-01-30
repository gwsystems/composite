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
#include <cbuf_mgr.h>
#include <cbuf_vect.h>
#include <cos_vect.h>
#include <cos_debug.h>
#include <cos_alloc.h>
#include <valloc.h>

#define CSLAB_ALLOC(sz)   alloc_page()
#define CSLAB_FREE(x, sz) free_page(x)
#include <cslab.h>

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
PERCPU_VAR(cbuf_alloc_freelists);

/*** Slow paths for each cbuf operation ***/

/* 
 * Precondition: cbuf lock is taken.
 *
 * A component has tried to map a cbuf_t to a buffer, but that cbuf
 * isn't mapping into the component.  The component's cache of cbufs
 * had a miss.
 */
int 
__cbuf_2buf_miss(int cbid, int len)
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
	mc = cbuf_vect_lookup_addr(cbid_to_meta_idx(cbid));
	/* ...have to expand the cbuf_vect */
	if (unlikely(!mc)) {
		if (cbuf_vect_expand(&meta_cbuf, cbid_to_meta_idx(cbid))) BUG();    //???
		mc = cbuf_vect_lookup_addr(cbid_to_meta_idx(cbid));
		assert(mc);
	}
	ret = cbuf_retrieve(cos_spd_id(), cbid, len);
	if (unlikely(ret < 0 || mc->sz < (len >> PAGE_ORDER))) return -1;
	assert(CBUFM_GET_PTR(mc));

	return 0;
}

static inline int
__cbufp_alloc_slow(int cbid, int size, int *len, int *error)
{
	int amnt = 0, i;
	static struct cbufp_shared_page *csp = NULL;

	assert(cbid <= 0);
	if (cbid == 0) {
		struct cbuf_meta *cm;
		struct cbufp_ring_element el;
		if (!csp) csp = (struct cbufp_shared_page*)cbufp_map_collect(cos_spd_id());
		/* Do a garbage collection */
		amnt = cbufp_collect(cos_spd_id(), size);
		if (amnt < 0) {
			*error = 1;
			return -1;
		}
		assert((unsigned)amnt <= CSP_BUFFER_SIZE);

		if (amnt > 0) {
			if (CK_RING_DEQUEUE_SPSC(cbufp_ring, &csp->ring, &el)) {
				cbid = el.cbid;
				/* own the cbuf we just collected */
				cm = cbuf_vect_lookup_addr(cbid_to_meta_idx(cbid));
				assert(cm);
				assert(cm->cbid.cbid == cbid);
				/* (should be atomic) */
				CBUF_SET_TOUCHED(cm);
				assert(CBUFM_GET_REFCNT(cm) < CBUF_REFCNT_MAX);
				CBUFM_INC_REFCNT(cm);
			} else {
				/* Someone stole the cbufs I collected! */
				amnt = 0;
			}
		}
		/* ...add the rest back into freelists. construct a temporal freelist 
		 * first, then add this temporal list to freelist atomically*/
		struct cbuf_meta *meta, *tail, *head, old_head, new_head;
		int cb, idx;
		if (CK_RING_DEQUEUE_SPSC(cbufp_ring, &csp->ring, &el)) {
			cb = el.cbid;
			idx = cbid_to_meta_idx(cb);
			assert(idx > 0);
			head = tail = cbuf_vect_lookup_addr(idx);
			assert(!((int)tail & CBUFM_NEXT_MASK));
			for(i = 2; i < amnt; ++i) {
				if (!CK_RING_DEQUEUE_SPSC(cbufp_ring, &csp->ring, &el)) break;
				cb = el.cbid;
				idx = cbid_to_meta_idx(cb);
				assert(idx > 0);
				meta = cbuf_vect_lookup_addr(idx);
				assert(!((int)meta & CBUFM_NEXT_MASK));
				CBUFM_SET_NEXT(meta, head);
				head = meta;
			}
			unsigned long long *target, *old, *update;
			meta = __cbuf_freelist_get(size);
			CBUFM_SET_NEXT(tail, CBUFM_GET_NEXT(meta));
			target = (unsigned long long *)(&meta->next_flag);
			old    = (unsigned long long *)(&old_head.next_flag);
			update = (unsigned long long *)(&new_head.next_flag);
			new_head.next_flag = head;
			do {
				*old = *target;
				new_head.cbid.tag = meta->cbid.tag+1;
			} while(unlikely(!cos_dcas(target, *old, *update)));
		}
	}
	/* Nothing collected...allocate a new cbufp! */
	if (amnt == 0) {
		cbid = cbuf_create(cos_spd_id(), size, cbid*-1);
		if (cbid == 0) assert(0);
	} 
	/* TODO update correctly */
	*len = 1;
	return cbid;
}

/* 
 * Precondition: cbuf lock is taken.
 */
struct cbuf_meta *
__cbuf_alloc_slow(int size, int *len)
{
	struct cbuf_meta *cm = NULL;
	int cbid;
	int cnt;

	cnt = cbid = 0;
	do {
		int error = 0;
		cbid = __cbufp_alloc_slow(cbid, size, len, &error);
		if (unlikely(error)) goto done;
		if (cbid < 0 && cbuf_vect_expand(&meta_cbuf, cbid_to_meta_idx(cbid*-1)) < 0) goto done;
		/* though it's possible this is valid, it probably
		 * indicates an error */
		assert(cnt++ < 10);
	} while (cbid < 0);
	assert(cbid);
	cm   = cbuf_vect_lookup_addr(cbid_to_meta_idx(cbid));
	assert(cm && CBUFM_GET_PTR(cm));
	assert(cm && CBUFM_GET_REFCNT(cm));
done:   
	return cm;
}

CCTOR static void
cbuf_init(void)
{
	//initialize freelist head
	return ;  //???
}

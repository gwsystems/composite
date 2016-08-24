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
	mc = cbuf_vect_lookup_addr(cbid);
	/* ...have to expand the cbuf_vect */
	if (unlikely(!mc)) {
		if (cbuf_vect_expand(&meta_cbuf, cbid_to_meta_idx(cbid))) BUG(); 
		mc = cbuf_vect_lookup_addr(cbid);
		assert(mc);
	}
	ret = cbuf_retrieve(cos_spd_id(), cbid, len);
	if (unlikely(ret < 0 || mc->sz < (len >> PAGE_ORDER))) return -1;
	assert(CBUF_PTR(mc));

	return 0;
}

static inline int
__cbufp_alloc_slow(int cbid, int size, int *len, int *error)
{
	int amnt = 0, i;
	static struct cbuf_shared_page *csp = NULL;

	assert(cbid <= 0);
	if (cbid == 0) {
		struct cbuf_meta *cm;
		struct cbuf_ring_element el;
		if (!csp) csp = (struct cbuf_shared_page*)cbuf_map_collect(cos_spd_id());
		/* Do a garbage collection */
		amnt = cbuf_collect(cos_spd_id(), size);
		if (amnt < 0) {
			*error = 1;
			return -1;
		}
		assert((unsigned)amnt <= CSP_BUFFER_SIZE);

		if (amnt > 0) {
			if (CK_RING_DEQUEUE_SPSC(cbuf_ring, &csp->ring, &el)) {
				cbid = el.cbid;
				/* own the cbuf we just collected */
				cm = cbuf_vect_lookup_addr(cbid);
				assert(cm);
				assert(cm->cbid_tag.cbid == (unsigned)cbid);
				assert(CBUF_REFCNT(cm) < CBUF_REFCNT_MAX);
				CBUF_REFCNT_ATOMIC_INC(cm);
			} else {
				/* Someone stole the cbufs I collected! */
				amnt = 0;
			}
		}

		/* ...add the rest back into freelists. construct a temporary freelist 
		 * first, then add this temporal list to freelist atomically*/
		struct cbuf_meta *meta, *tail, *head, old_head, new_head;
		int cb;
		if (CK_RING_DEQUEUE_SPSC(cbuf_ring, &csp->ring, &el)) {
			cb = el.cbid;
			head = tail = cbuf_vect_lookup_addr(cb);
			assert(!tail->next);
			for(i = 2; i < amnt; ++i) {
				if (!CK_RING_DEQUEUE_SPSC(cbuf_ring, &csp->ring, &el)) break;
				cb = el.cbid;
				meta = cbuf_vect_lookup_addr(cb);
				assert(!meta->next);
				meta->next = head;
				head = meta;
			}

			unsigned long long *target, *old, *update;
			meta = __cbuf_freelist_get(size);
			tail->next = meta->next;
			target = (unsigned long long *)(&meta->next);
			old    = (unsigned long long *)(&old_head.next);
			update = (unsigned long long *)(&new_head.next);
			new_head.next = head;
			do {
				*old = *target;
				new_head.cbid_tag.tag = meta->cbid_tag.tag+1;
			} while(unlikely(!cos_dcas(target, *old, *update)));
		}
	}
	/* Nothing collected...allocate a new cbuf! */
	if (amnt == 0) {
		cbid = cbuf_create(cos_spd_id(), size, cbid * -1);
		assert(cbid != 0);
	}
	return cbid;
}

struct cbuf_meta *
__cbuf_alloc_slow(int size, int *len, unsigned int flag)
{
	struct cbuf_meta *cm = NULL;
	int cbid;
	int cnt;

	cnt = cbid = 0;
	do {
		int error = 0;
		if (flag & CBUF_EXACTSZ) cbid = cbuf_create(cos_spd_id(), size, cbid * -1);
		else                     cbid = __cbufp_alloc_slow(cbid, size, len, &error);
		if (unlikely(error)) goto done;
		if (cbid < 0 && cbuf_vect_expand(&meta_cbuf, cbid_to_meta_idx(cbid * -1)) < 0) goto done;
		/* though it's possible this is valid, it probably
		 * indicates an error */
		assert(cnt++ < 10);
	} while (cbid < 0);
	assert(cbid);
	cm   = cbuf_vect_lookup_addr(cbid);
	assert(cm && CBUF_PTR(cm));
	assert(cm && CBUF_REFCNT(cm));
	/* TODO update correctly */
	*len = 1;
done:   
	return cm;
}

CCTOR static void
cbuf_init(void)
{
	//initialize freelist head
	return ;  //???
}


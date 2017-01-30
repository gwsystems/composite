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
#include <sched.h>

/* 
 * All of the structures that must be compiled (only once) into each
 * component that uses cbufs.
 *
 * I'd like to avoid having to make components initialize this data,
 * so try and use structures that by default are initialized as all
 * zero. 
 */

#define CBUF_RING_TAKE()      do { if (sched_component_take(cos_spd_id()))    BUG(); } while(0)
#define CBUF_RING_RELEASE()   do { if (sched_component_release(cos_spd_id())) BUG(); } while(0)
CVECT_CREATE_STATIC(meta_cbuf);
PERCPU_VAR(cbuf_alloc_freelists);

/*** Slow paths for each cbuf operation ***/

/* 
 * A component has tried to map a cbuf_t to a buffer, but that cbuf
 * isn't mapping into the component.  The component's cache of cbufs
 * had a miss.
 */
int 
__cbuf_2buf_miss(unsigned int cbid, int len)
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
	ret = cbuf_retrieve(cos_spd_id(), cbid, (unsigned long)len);
	if (unlikely(ret < 0 || mc->sz < (len >> PAGE_ORDER))) return -1;
	assert(CBUF_PTR(mc));

	return 0;
}

/*
 * Return the first one to client and put the rest to the 
 * free-list. csp is the shared page between this client and manager. 
 *
 * Synchronization around this shared ring buffer.  This is a single
 * consumer single producer ring buffer. To protect this ring buffer, 
 * I just simply use a lock.
 */
static struct cbuf_meta *
__cbuf_get_collect(struct cbuf_shared_page *csp, unsigned long size, unsigned int flag)
{
	struct cbuf_ring_element el;
	struct cbuf_meta *ret_cm, *cm;
	struct cbuf_meta *tail, *head;

	assert(csp);

	CBUF_RING_TAKE();
	do {
		assert(&csp->ring);
		ret_cm = NULL;
		if (CK_RING_DEQUEUE_SPSC(cbuf_ring, &csp->ring, &el)) {
			/* own the cbuf we just collected */
			ret_cm = cbuf_vect_lookup_addr(el.cbid);
			assert(ret_cm && ret_cm->cbid_tag.cbid == el.cbid);
		} else {
			break;
		}
	} while(!__cbuf_try_take(ret_cm, flag));

	/* 
	 * ...add the rest back into freelists. construct a temporary freelist 
	 * first, then add this temporal list to freelist atomically
	 */
	if (CK_RING_DEQUEUE_SPSC(cbuf_ring, &csp->ring, &el)) {
		head = tail = cbuf_vect_lookup_addr(el.cbid);
		assert(head && head->cbid_tag.cbid == el.cbid);

		while(CK_RING_DEQUEUE_SPSC(cbuf_ring, &csp->ring, &el)) {
			cm       = cbuf_vect_lookup_addr(el.cbid);
			assert(cm && cm->cbid_tag.cbid == el.cbid);
			cm->next = head;
			head     = cm;	
		}
		CBUF_RING_RELEASE();
		__cbuf_freelist_push(size, head, tail);
	} else {
		CBUF_RING_RELEASE();;
	}

	return ret_cm;
}

struct cbuf_meta *
__cbuf_alloc_slow(unsigned long size, int *len, unsigned int flag)
{
	int amnt, cbid;
	struct cbuf_meta *ret_cm;
	static struct cbuf_shared_page *csp = NULL;

	csp = (struct cbuf_shared_page*)cbuf_map_collect(cos_spd_id());
again:
	ret_cm = NULL;
	cbid = 0;

	/* Attempt garbage collection if this is not an exact size cbuf */
	if (!(flag & CBUF_EXACTSZ)) {
		ret_cm = __cbuf_get_collect(csp, size, flag);
		if (ret_cm) goto done;

		amnt   = cbuf_collect(cos_spd_id(), size);
		if (unlikely(amnt < 0)) goto done;
		assert((unsigned)amnt <= CSP_BUFFER_SIZE);

		ret_cm = __cbuf_get_collect(csp, size, flag);
		if (!ret_cm) ret_cm = __cbuf_freelist_pop(size, flag);
		else 	assert(ret_cm && CBUF_PTR(ret_cm));
	}
	
	/* Nothing collected...allocate a new cbuf! */
	if (!ret_cm) {
		cbid = cbuf_create(cos_spd_id(), size, cbid*-1);
		/* We return from being blocked */
		if (cbid == 0) {
			goto again;
		} else if (cbid < 0 ) {
			if (cbuf_vect_expand(&meta_cbuf, cbid_to_meta_idx(cbid*-1)) < 0) goto done;
			cbid = cbuf_create(cos_spd_id(), size, cbid*-1);
		}
		assert(cbid > 0);
		ret_cm = cbuf_vect_lookup_addr((unsigned int)cbid);
		if (unlikely(flag)) CBUF_FLAG_ADD(ret_cm, flag);
	}
	assert(ret_cm && CBUF_PTR(ret_cm));
done:   
	/* TODO update correctly */
	*len = 1;
	return ret_cm;
}

CCTOR static void
cbuf_init(void)
{
	struct cbuf_freelist *fl = PERCPU_GET(cbuf_alloc_freelists);
	struct cbuf_meta *m;
	int i;
	for (i = 0 ; i < CBUF_MAX_NSZ ; i++) {
		m = &(fl->freelist_head[i]);
		m->next = m;
	}
	return ;  
}


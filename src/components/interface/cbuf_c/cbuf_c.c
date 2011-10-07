/**
 * Copyright 2010 by The George Washington University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gparmer@gwu.edu, 2010
 */

#include <cos_component.h>
#include <print.h>
#include <cbuf.h>
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

CBUF_VECT_CREATE_STATIC(meta_cbuf);
COS_VECT_CREATE_STATIC(slab_descs);
struct cbuf_slab_freelist slab_freelists[N_CBUF_SLABS];

/* 
 * A component has tried to map a cbuf_t to a buffer, but that cbuf
 * isn't mapping into the component.  The component's cache of cbufs
 * had a miss.
 */
int 
cbuf_cache_miss(int cbid, int idx, int len)
{
	union cbuf_meta mc;
	void *h;
	/* DOUT("cbid is %d idx is %d len is %d\n",cbid, idx, len); */

	CBUF_RELEASE();
	h = cbuf_c_retrieve(cos_spd_id(), cbid, len);
	CBUF_TAKE();
	if (!h) {
		//valloc_free(cos_spd_id(), cos_spd_id(),h, 1);
		BUG();
		/* Illegal cbid or length!  Bomb out. */
		return -1;
	}

	mc.c.ptr    = (long)h >> PAGE_ORDER;
	mc.c.obj_sz = len >> CBUF_OBJ_SZ_SHIFT;

	/* This is the commit point */
	DOUT("cache miss: meta_cbuf is at %p, h is %p\n", &meta_cbuf, h);
	assert((void *)mc.c_0.v);
	cbuf_vect_add_id(&meta_cbuf, (void *)mc.c_0.v, cbid_to_meta_idx(cbid));
	cbuf_vect_add_id(&meta_cbuf, (void *)(unsigned long)cos_get_thd_id(), cbid_to_meta_idx(cbid)+1);

	return 0;
}

void
cbuf_slab_cons(struct cbuf_slab *s, int cbid, void *page, 
	       int obj_sz, struct cbuf_slab_freelist *freelist)
{
	s->cbid = cbid;
	s->mem = page;
	s->obj_sz = obj_sz;
	memset(&s->bitmap[0], 0xFFFFFFFF, sizeof(u32_t)*SLAB_BITMAP_SIZE);
	s->nfree = s->max_objs = PAGE_SIZE/obj_sz; /* not a perf sensitive path */
	s->flh = freelist;
	INIT_LIST(s, next, prev);
	/* FIXME: race race race */
	slab_add_freelist(s, freelist);

	return;
}


struct cbuf_slab *
cbuf_slab_alloc(int size, struct cbuf_slab_freelist *freelist)
{
	struct cbuf_slab *s, *ret = NULL;
	struct cbuf_slab *exist;
	/* struct cbuf_slab *dup = NULL; */
	void *addr;
	int cbid;
	int cnt;

	/* DOUT("Relinquish bit :: %d\n",cos_comp_info.cos_tmem_relinquish[COMP_INFO_TMEM_CBUF_RELINQ]); */
	s = malloc(sizeof(struct cbuf_slab));
	if (!s) return NULL;
	if (!freelist) goto err;

	/* union cbuf_meta mc; */

	/* DOUT("meta_cbuf is %p\n",&meta_cbuf); */
	cnt = 0;
	cbid = 0;
	do {
		CBUF_RELEASE();
		cbid = cbuf_c_create(cos_spd_id(), size, cbid*-1);
		CBUF_TAKE();
		if (cbid < 0) {
			if (cbuf_vect_expand(&meta_cbuf, cbid*-1) < 0) goto err;
		}
		/* FIXME: once everything is well debugged, remove this check */
		assert(cnt++ < 10);
	} while (cbid < 0);

	addr = cbuf_vect_addr_lookup(&meta_cbuf, cbid_to_meta_idx(cbid));
	if (!addr) goto err;

	/* DOUT("create: meta_cbuf is at %p\n", &meta_cbuf); */

	// Check if the allocated cbuf item is the used one and if it is still on local free_list

	/* 
	 * See __cbuf_alloc and cbuf_slab_free.  It is possible that a
	 * slab descriptor will exist for a piece of cbuf memory
	 * _before_ it is allocated because it is actually from a
	 * previous cbuf.  If this is the case, then we should take
	 * over the slab, and use it for this cbuf.
	 */
	exist = cos_vect_lookup(&slab_descs, (u32_t)addr>>PAGE_ORDER);
	if (exist) {
		slab_deallocate(exist, freelist);
	}
	assert(!cos_vect_lookup(&slab_descs, (u32_t)addr>>PAGE_ORDER));

	cos_vect_add_id(&slab_descs, s, (long)addr>>PAGE_ORDER);
	cbuf_slab_cons(s, cbid, addr, size, freelist);

	ret = s;

done:   
	return ret;
err:    
	/* valloc_free(cos_spd_id(), cos_spd_id(), h, 1); */
	free(s);
	goto done;
}

void
cbuf_slab_free(struct cbuf_slab *s)
{
	struct cbuf_slab_freelist *freelist;
	union cbuf_meta cm;
	DOUT("call slab_free(s)...\n");
	/* FIXME: soooo many races */
	freelist = s->flh;
	assert(freelist);

	/* 
	 * A good question: When is the slab deallocated???  See
	 * cbuf.h:__cbuf_alloc for an explanation.
	 */
	/* slab_add_freelist(s, freelist); */

	/* clear IN_USE bit */
	cm.c_0.v = (u32_t)cbuf_vect_lookup(&meta_cbuf, cbid_to_meta_idx(s->cbid));
	cm.c.flags &= ~CBUFM_IN_USE;
	assert((void *)cm.c_0.v);
	cbuf_vect_add_id(&meta_cbuf, (void*)cm.c_0.v, cbid_to_meta_idx(s->cbid));

	DOUT("In cbuf_slab_free -- cbid is %d\n", s->cbid);

	if(cos_comp_info.cos_tmem_relinquish[COMP_INFO_TMEM_CBUF_RELINQ] == 1){
		assert(!CBUF_IN_USE(cm.c.flags));
		DOUT("need relinquish\n");
		
		CBUF_RELEASE();
		cbuf_c_delete(cos_spd_id(), s->cbid);
		CBUF_TAKE();
	}

	return;

}

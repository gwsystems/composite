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
	char *h;

	h = valloc_alloc(cos_spd_id(), cos_spd_id(), 1);
	assert(h);
	mc.c.ptr    = (long)h >> PAGE_ORDER;
	mc.c.obj_sz = len>>6;
	if (cbuf_c_retrieve(cos_spd_id(), cbid, len, h)) {
		valloc_free(cos_spd_id(), cos_spd_id(),h, 1);
		BUG();
		/* Illegal cbid or length!  Bomb out. */
		return -1;
	}
	/* This is the commit point */
	cbuf_vect_add_id(&meta_cbuf, (void*)mc.c_0.v, cbid);

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
	struct cbuf_slab *s = malloc(sizeof(struct cbuf_slab)), *ret = NULL;
	void *h;
	int cbid;
	int c_cbid;// combined id
	printc("5\n");
	if (!s) return NULL;
	union cbuf_meta mc;

	h = valloc_alloc(cos_spd_id(), cos_spd_id(), 1);

	assert(h); 

	cbid = cbuf_c_create(cos_spd_id(), size, h);
	printc("6\n");
	printc("cbid is %d\n",cbid);
	printc("thdid is %d\n",cos_get_thd_id());
	if (cbid < 0) goto err;

	mc.c.ptr    = (long)h >> PAGE_ORDER;
	mc.c.obj_sz = size>>6;
	mc.c.thd_id = cos_get_thd_id();

	c_cbid = cbid*2;
	/* printc("added address: %p\n",(void*)mc.c_0.v); */
	cbuf_vect_add_id(&meta_cbuf, (void*)mc.c_0.thd_id, c_cbid);
	cbuf_vect_add_id(&meta_cbuf, (void*)mc.c_0.v, c_cbid-1);

	cos_vect_add_id(&slab_descs, s, (long)h>>PAGE_ORDER);
	
	cbuf_slab_cons(s, cbid, h, size, freelist);

	freelist->velocity = 0;
	ret = s;
done:   
	return ret;
err:    
	valloc_free(cos_spd_id(), cos_spd_id(), h, 1);
	free(s);
	goto done;
}

void
cbuf_slab_free(struct cbuf_slab *s)
{
	struct cbuf_slab_freelist *freelist;

	/* FIXME: soooo many races */
	freelist = s->flh;
	assert(freelist);
	freelist->velocity--;
	if (freelist->velocity > SLAB_VELOCITY_THRESH) return;

	/* Have we freed the configured # in a row? Return the page. */
	slab_rem_freelist(s, freelist);
	assert(s->nfree = (PAGE_SIZE/s->obj_sz));
	
	cos_vect_del(&slab_descs, (long)s->mem>>PAGE_ORDER);
	
	cbuf_c_delete(cos_spd_id(), s->cbid);
	valloc_free(cos_spd_id(), cos_spd_id(), s->mem, 1);
	free(s);

	return;
}


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
#include <cos_vect.h>

/* 
 * All of the structures that must be compiled (only once) into each
 * component that uses cbufs.
 *
 * I'd like to avoid having to make components initialize this data,
 * so try and use structures that by default are initialized as all
 * zero. 
 */

COS_VECT_CREATE_STATIC(meta_cbuf);
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

	h = cos_get_vas_page();
	mc.c.ptr    = (long)h >> PAGE_ORDER;
	mc.c.obj_sz = len;
	if (cbuf_c_retrieve(cos_spd_id(), cbid, len, h)) {
		/* Illegal cbid or length!  Bomb out. */
		return -1;
	}
	/* This is the commit point */
	cos_vect_add_id(&meta_cbuf, (void*)mc.v, cbid);

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

	if (!s) return NULL;
	h = cos_get_vas_page();
	cbid = cbuf_c_create(cos_spd_id(), size, h);
	if (cbid < 0) goto err;
	cos_vect_add_id(&slab_descs, s, (long)h>>PAGE_ORDER);
	cbuf_slab_cons(s, cbid, h, size, freelist);
	freelist->velocity = 0;
	ret = s;
done:   
	return ret;
err:    
	cos_release_vas_page(h);
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
	
	/* FIXME: reclaim heap VAS! */
	cos_vect_del(&slab_descs, (long)s->mem>>PAGE_ORDER);
	cbuf_c_delete(cos_spd_id(), s->cbid);
	free(s);

	return;
}


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
	void *h;
	/* printc("cbid is %d idx is %d\n",cbid, idx); */

//	h = valloc_alloc(cos_spd_id(), cos_spd_id(), 1);
//	assert(h);
//	mc.c.ptr    = (long)h >> PAGE_ORDER;
//	mc.c.obj_sz = len>>6;

	h = cbuf_c_retrieve(cos_spd_id(), cbid, len);
	if (!h) {
		//valloc_free(cos_spd_id(), cos_spd_id(),h, 1);
		BUG();
		/* Illegal cbid or length!  Bomb out. */
		return -1;
	}

	mc.c.ptr    = (long)h >> PAGE_ORDER;
	mc.c.obj_sz = len>>6;

	/* This is the commit point */
	/* printc("miss: meta_cbuf is at %p, h is %p\n", &meta_cbuf, h); */
	cbuf_vect_add_id(&meta_cbuf, (void*)mc.c_0.v, (cbid-1)*2);
	cbuf_vect_add_id(&meta_cbuf, cos_get_thd_id(), (cbid-1)*2+1);
	int i;
	for(i=0;i<20;i++)
		printc("i:%d %p\n",i,cbuf_vect_lookup(&meta_cbuf, i));
	
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
	struct cbuf_slab *dup = NULL;
	void *addr;
	int cbid;
	int cnt;

	if (!s || !freelist) goto err;

	/* union cbuf_meta mc; */

	/* printc("meta_cbuf is %p\n",&meta_cbuf); */
	cnt = 0;
	cbid = 0;
	do {
		cbid = cbuf_c_create(cos_spd_id(), size, cbid*-1);
		if (cbid < 0) {
			if (cbuf_vect_expand(&meta_cbuf, cbid*-1) < 0) goto err;
		}
		/* FIXME: once everything is well debugged, remove this check */
		assert(cnt++ < 10);
	} while (cbid < 0);
	/* printc("slab_alloc -- cbid is %d\n",cbid); */

	/* int i; */
	/* for(i=0;i<20;i++) */
	/* 	printc("i:%d %p\n",i,cbuf_vect_lookup(&meta_cbuf, i)); */

	/* printc("create: meta_cbuf is at %p\n", &meta_cbuf); */
	addr = cbuf_vect_addr_lookup(&meta_cbuf, (cbid-1)*2);
	if (!addr) goto err;

	// Check if the allocated cbuf item is the used one and if it is still on local free_list


	struct cbuf_slab *exist = cos_vect_lookup(&slab_descs, (long)addr>>PAGE_ORDER);

	if(!exist){
		printc("not exist???\n");
		cos_vect_add_id(&slab_descs, s, (long)addr>>PAGE_ORDER);
		cbuf_slab_cons(s, cbid, addr, size, freelist);
		ret = s;
	}
	else
	{
		printc("exist:: %d\n", exist->cbid);
		ret = exist;
	}

	/* printc("1\n"); */
	/* if (!freelist->list) goto err; */

	/* // Try to remove all duplicate stayed cbufs */
	/* cnt = 0; */
	/* if (cbid == freelist->list->cbid) cnt++; */

	/* for (dup = FIRST_LIST(freelist->list, next, prev); */
	/*      dup != freelist->list; */
	/*      dup = FIRST_LIST(dup, next, prev)) { */
	/* 	printc("dup's id %d\n",dup->cbid); */
	/* 	if (dup->cbid == cbid) { */
	/* 		cnt++; */
	/* 		printc("3\n"); */
	/* 		//break; */
	/* 	} */
	/* } */
	/* while (cnt-- > 1) */
	/* { */
	/* 	assert(freelist->npages > 0); */
	/* 	cos_vect_del(&slab_descs, (long)addr>>PAGE_ORDER); */
	/* 	slab_rem_freelist(dup, freelist); */
	/* } */

	/* assert(cnt == 0); */
	/* ret = s; */
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
	printc("call slab_free(s)...\n");	
	/* FIXME: soooo many races */
	freelist = s->flh;
	assert(freelist);

	/* clear IN_USE bit */
	cm.c_0.v = (u32_t)cbuf_vect_lookup(&meta_cbuf, (s->cbid - 1) * 2);
	cm.c.flags &= ~CBUFM_IN_USE;
	cbuf_vect_add_id(&meta_cbuf, (void*)cm.c_0.v, (s->cbid - 1 ) * 2);
	/* check relinquish here! */
	/* printc("cm.c.flags & CBUFM_IN_USE are %p and %p\n",cm.c.flags , CBUFM_IN_USE); */
	/* printc("cm.c.flags & CBUFM_RELINQUISH are %p and %p\n",cm.c.flags , CBUFM_RELINQUISH); */

	if (!(cm.c.flags & CBUFM_RELINQUISH))
	{
		printc("No need to relinquish~~~\n");	
		return;
	}

	if(cbuf_c_del_elig(cos_spd_id(), s->cbid))
	{
		printc("Not on freelist anymore\n");
		/* Have we freed the configured # in a row? Return the page. */
		slab_rem_freelist(s, freelist);
		assert(s->nfree == (PAGE_SIZE/s->obj_sz));

		cos_vect_del(&slab_descs, (long)s->mem>>PAGE_ORDER);
		/* Has the cbuf mgr asked for the cbuf? Return the page. Relinqush to return to mgr! */
		cbuf_c_delete(cos_spd_id(), s->cbid, 1);
		free(s);  // created at slab_alloc
	}
	else
	{
		printc("Still on freelist\n");
		cbuf_c_delete(cos_spd_id(), s->cbid, 0);
	}
	
	return;

	/* else */
	/* { */
	/* 	printc("truly delete from free list\n"); */
	/* 	cos_vect_del(&slab_descs, (long)s->mem>>PAGE_ORDER); */
	/* 	slab_rem_freelist(s, freelist); */
	/* 	assert(s->nfree == (PAGE_SIZE/s->obj_sz)); */
	/* 	/\* printc("s->mem %p\n",(long)s->mem>>PAGE_ORDER); *\/ */
	/* 	free(s);  // created at slab_alloc */
	/* } */
/* done: */
/* 	printc("\nthd %d slab_free done here...\n\n", cos_get_thd_id()); */
/* 	return; */
}


/**
 * Copyright 2010 by The George Washington University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gparmer@gwu.edu, 2010
 */

#include <cos_component.h>
#include <sched.h>
#include <cos_synchronization.h>
#include <print.h>
#include <cos_alloc.h>
#include <cos_map.h>
#include <cos_list.h>
#include <mem_mgr_large.h>
#include <valloc.h>

#include <tmem.h>
#include <cbuf_c.h>
//#define PRINCIPAL_CHECKS

#define DEFAULT_TARGET_ALLOC 10

#define CBUF_IN_USE(flags) (flags & CBUFM_IN_USE)
		
struct cos_cbuf_item all_cbuf_list[MAX_NUM_CBUFS];

COS_MAP_CREATE_STATIC(cb_ids);

void mgr_map_client_mem(struct cos_cbuf_item *cci, struct spd_tmem_info *sti)
{
	char *l_addr, *d_addr;

	assert(sti && cci);
	assert(EMPTY_LIST(cci, next, prev));

	/* TODO: multiple pages cbuf! */
	l_addr = cci->desc.addr;
	d_addr = valloc_alloc(cos_spd_id(), sti->spdid, 1);
	assert(d_addr && l_addr); 

	/* ...map it into the requesting component */
	if (!mman_alias_page(cos_spd_id(), (vaddr_t)l_addr, sti->spdid, (vaddr_t)d_addr)) goto err;
	cci->desc.owner.addr = (vaddr_t)d_addr;

	// add the cbuf to shared vect here? now we do it in the client.
done:
	return;
err:
	printc("Cbuf mgr: Cannot alias page to client!\n");
	mman_release_page(cos_spd_id(), (vaddr_t)l_addr, 0);
	valloc_free(cos_spd_id(), cos_spd_id(), l_addr, 1);
	goto done;
}

void __cbuf_c_delete(struct spd_tmem_info *sti, int cbid);

tmem_item * mgr_get_client_mem(struct spd_tmem_info *sti)
{
	spdid_t s_spdid;
	struct cos_cbuf_item * cci = NULL, *list;
	
	assert(sti);
	s_spdid = sti->spdid;

	list = &spd_tmem_info_list[s_spdid].tmem_list;

	union cbuf_meta cm;
	/* Go through the allocated cbufs, and see if any are not in use... */
	for (cci = FIRST_LIST(list, next, prev) ; 
	     cci != list ; 
	     cci = FIRST_LIST(cci, next, prev)) {
		cm.v = (u32_t)((sti->ci)[(cci->desc.cbid & COS_VECT_MASK)].val);
		if (!CBUF_IN_USE(cm.c.flags)) break;
	}
	if (cci == list) return NULL;

	/* ...and remove it if it is not. */
	__cbuf_c_delete(sti, cci->desc.cbid);

	cci->parent_spdid = 0;
	
	// Clear our memory to prevent leakage
	memset(cci->desc.addr, 0, PAGE_SIZE);
	
	DOUT("Removing from local list\n");

	REM_LIST(cci, next, prev);
	/* TODO: move all of this into the tmem generic code just like the ++s */
	sti->num_allocated--;
	if (sti->num_allocated == 0) empty_comps++;
	if (sti->num_allocated >= sti->num_desired) over_quota_total--;
	assert(sti->num_allocated == tmem_num_alloc_stks(s_spdid));

	return cci;
}

void spd_mark_relinquish(struct spd_tmem_info *sti)
{
	struct cos_cbuf_item *cci;
	union cbuf_meta cm;

	for(cci = FIRST_LIST(&sti->tmem_list, next, prev);
	    cci != &sti->tmem_list;
	    cci = FIRST_LIST(cci, next, prev)){
		cm.v = (u32_t)((sti->ci)[(cci->desc.cbid & COS_VECT_MASK)].val);
		cm.c.flags |= CBUFM_RELINQUISH;
		(sti->ci)[(cci->desc.cbid & COS_VECT_MASK)].val = (void *)cm.v;
	}
}

void spd_unmark_relinquish(struct spd_tmem_info *sti)
{
	struct cos_cbuf_item *cci;
	union cbuf_meta cm;

	for(cci = FIRST_LIST(&sti->tmem_list, next, prev);
	    cci != &sti->tmem_list;
	    cci = FIRST_LIST(cci, next, prev)){
		cm.v = (u32_t)((sti->ci)[(cci->desc.cbid & COS_VECT_MASK)].val);
		cm.c.flags &= ~CBUFM_RELINQUISH;
		(sti->ci)[(cci->desc.cbid & COS_VECT_MASK)].val = (void *)cm.v;
	}
}

u32_t resolve_dependency(struct spd_tmem_info *sti, int skip_stk)
{
	return 0;
}

vaddr_t
cbuf_c_register(spdid_t spdid)
{
	struct spd_tmem_info *sti;
	vaddr_t p, mgr_addr;

	sti = get_spd_info(spdid);
	
	mgr_addr = (vaddr_t)alloc_page();
	p = (vaddr_t)valloc_alloc(cos_spd_id(), spdid, 1);
	if (p !=
	    (mman_alias_page(cos_spd_id(), mgr_addr, spdid, p))) {
		printc("mapped faied p is %p\n",(void *)p);
		return -1;
	}
	sti->ci = (struct cbuf_vect_intern_struct *)mgr_addr;
	sti->managed = 1;

	return p;
}

int
cbuf_c_create(spdid_t spdid, int size, void * page)
{
	int ret = -1, cbid;
	struct spd_tmem_info *sti;
	struct cos_cbuf_item *cbuf_item;
	struct cb_desc *d;

	TAKE();

	sti = get_spd_info(spdid);
	
	/* Make sure we have access to the component shared page */
//	if (!SPD_IS_MANAGED(sti)) map_cbuf_vect_info(spdid);
	assert(SPD_IS_MANAGED(sti));

	/* call trasient memory grant! */
	cbuf_item = tmem_grant(sti);

	assert(cbuf_item);

	d = &cbuf_item->desc;
	d->principal  = cos_get_thd_id();
	d->obj_sz     = size;
//	d->addr       = l_addr;
	d->owner.spd  = sti->spdid;
//	d->owner.addr = (vaddr_t)d_addr;
	d->owner.cbd  = d;
	INIT_LIST(&d->owner, next, prev);
	cbid          = cos_map_add(&cb_ids, d);
	ret = d->cbid = cbid;

	RELEASE();
	return ret;
}

void __cbuf_c_delete(struct spd_tmem_info *sti, int cbid)
{
	struct cb_desc *d;
	struct cb_mapping *m;
	
	printc("deleting, before: %p\n",sti->ci[cbid & COS_VECT_MASK].val);
	sti->ci[cbid & COS_VECT_MASK].val = (void*)COS_VECT_INIT_VAL;
	printc("deleting, after: %p\n",sti->ci[cbid & COS_VECT_MASK].val);

	d = cos_map_lookup(&cb_ids, cbid);
	if (!d) goto done;
	/* should be conditional on the principal??? */
	if (d->owner.spd != sti->spdid) goto done;
	cos_map_del(&cb_ids, cbid);
	/* mapping model will release all child mappings */
	DOUT("Releasing cbuf\n");
	mman_release_page(cos_spd_id(), (vaddr_t)d->addr, 0);
	valloc_free(cos_spd_id(), sti->spdid, (void *)(d->owner.addr), 1);

	m = FIRST_LIST(&d->owner, next, prev);
	while (m != &d->owner) {
		/* remove from the vector as well! */
		sti = get_spd_info(m->spd);
		sti->ci[cbid & COS_VECT_MASK].val = (void*)COS_VECT_INIT_VAL;
		valloc_free(cos_spd_id(), m->spd, (void *)(m->addr), 1);
		struct cb_mapping *n = FIRST_LIST(m, next, prev);
		REM_LIST(m, next, prev);
		free(m);
		m = n;
	}
done:
	return;
}
/* 
 * FIXME: 1) reference counting so that components can maintain the
 * buffer if they please, 2) asynchronous (shmmem) notification of cb
 * deallocation.
 */
void
cbuf_c_delete(spdid_t spdid, int cbid)
{
	struct spd_tmem_info *sti;
	TAKE();
//	printc("start returning!\n");
	sti = get_spd_info(spdid);
	assert(sti);

	DOUT("Return of s_spdid is: %d from thd: %d\n", s_spdid,
	     cos_get_thd_id());

	//__cbuf_c_delete(sti, cbid);
	return_tmem(sti);
//	printc("finished return!");
	RELEASE();
}

int
cbuf_c_retrieve(spdid_t spdid, int cbid, int len, void *page)
{
	int ret = -1;
	struct cb_desc *d;
	struct cb_mapping *m;

	TAKE();
	d = cos_map_lookup(&cb_ids, cbid);
	/* sanity and access checks */
	if (!d || d->obj_sz < len) goto done;
#ifdef PRINCIPAL_CHECKS
	if (d->principal != cos_get_thd_id()) goto done;
#endif
	m = malloc(sizeof(struct cb_mapping));
	if (!m) goto done;

	INIT_LIST(m, next, prev);
	m->cbd  = d;
	m->spd  = spdid;
	m->addr = (vaddr_t)page;

	if (!mman_alias_page(cos_spd_id(), (vaddr_t)d->addr, spdid, (vaddr_t)page)) goto err;
	ADD_LIST(&d->owner, m, next, prev);
	ret = 0;
done:
	RELEASE();
	return ret;
err:
	free(m);
	goto done;
}

void 
cos_init(void *d)
{
	/* lock_init(&l); */
	cos_map_init_static(&cb_ids);
	BUG_ON(cos_map_add(&cb_ids, NULL)); /* reserve id 0 */

	int i;
	struct cos_cbuf_item *cbuf_item;

	memset(spd_tmem_info_list, 0, sizeof(struct spd_tmem_info) * MAX_NUM_SPDS);
    
	for(i = 0; i < MAX_NUM_SPDS; i++){
		spd_tmem_info_list[i].spdid = i;    
		INIT_LIST(&spd_tmem_info_list[i].tmem_list, next, prev);
		INIT_LIST(&spd_tmem_info_list[i].bthd_list, next, prev);
	}

	free_tmem_list = NULL;
	INIT_LIST(&global_blk_list, next, prev);

	/* Initialize our free list */
	for(i = 0; i < MAX_NUM_CBUFS; i++){
                
		// put cbuf list is some known state
		cbuf_item = &(all_cbuf_list[i]);
		INIT_LIST(cbuf_item, next, prev);
		// allocate a page
		cbuf_item->desc.addr = valloc_alloc(cos_spd_id(), cos_spd_id(), 1);
		assert(cbuf_item->desc.addr); 
		/* get the page */
		if (!mman_get_page(cos_spd_id(), (vaddr_t)cbuf_item->desc.addr, 0)) {
			DOUT("<cbuf_mgr>: ERROR, could not allocate stack\n"); 
		} else {
			/* // figure out or location of the top of the stack */
			/* stk_item->stk = (struct cos_stk *)D_COS_STK_ADDR((char *)stk_item->hptr); */
			put_mem(cbuf_item);
		}
	}
	stacks_allocated = 0;

	// Map all of the spds we can into this component
	for (i = 0 ; i < MAX_NUM_SPDS ; i++) {
		spdid_t spdid = i;
		/* void *hp; */

		/* /\* hp = cos_get_vas_page(); *\/ */
		/* hp = valloc_alloc(cos_spd_id(), cos_spd_id(), 1); */
		/* spdid = cinfo_get_spdid(i); */
		/* if (!spdid) break; */

		/* if(cinfo_map(cos_spd_id(), (vaddr_t)hp, spdid)){ */
		/* 	DOUT("Could not map cinfo page for %d\n", spdid); */
		/* 	BUG(); */
		/* } */
		/* spd_tmem_info_list[spdid].ci = hp;  */
		spd_tmem_info_list[spdid].ci = NULL; 
		spd_tmem_info_list[spdid].managed = 1;

		/* DOUT("mapped -- id: %ld, hp:%x, sp:%x\n", */
		/*      spd_tmem_info_list[spdid].ci->cos_this_spd_id,  */
		/*      (unsigned int)spd_tmem_info_list[spdid].ci->cos_heap_ptr, */
		/*      (unsigned int)spd_tmem_info_list[spdid].ci->cos_stacks.freelists[0].freelist); */
    
		stacks_target += DEFAULT_TARGET_ALLOC;
		spd_tmem_info_list[spdid].num_allocated = 0;
		spd_tmem_info_list[spdid].num_desired = DEFAULT_TARGET_ALLOC;
		spd_tmem_info_list[spdid].num_blocked_thds = 0;
		spd_tmem_info_list[spdid].num_waiting_thds = 0;
		spd_tmem_info_list[spdid].num_glb_blocked = 0;
		spd_tmem_info_list[spdid].ss_counter = 0;
		spd_tmem_info_list[spdid].ss_max = MAX_NUM_CBUFS;
		empty_comps++;
	}
	over_quota_total = 0;
	over_quota_limit = MAX_NUM_CBUFS;
	/* DOUT("Done mapping components information pages!\n"); */
	/* DOUT("<stkmgr>: init finished\n"); */
	return;

}


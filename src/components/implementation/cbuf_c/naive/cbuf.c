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

//  all cbufs that created for this component
void mgr_map_client_mem(struct cos_cbuf_item *cci, struct spd_tmem_info *sti)
{
	char *l_addr, *d_addr;
	spdid_t d_spdid;
//	struct cb_desc *d;

	assert(sti && cci);
	assert(EMPTY_LIST(cci, next, prev));

	d_spdid = sti->spdid;

	/* TODO: multiple pages cbuf! */
	d_addr = valloc_alloc(cos_spd_id(), sti->spdid, 1);
	l_addr = cci->desc.addr;  //initialized in cos_init()

	assert(d_addr && l_addr); 

	/* ...map it into the requesting component */
	if (unlikely(!mman_alias_page(cos_spd_id(), (vaddr_t)l_addr, d_spdid, (vaddr_t)d_addr))) 
		goto err;
	/* printc("<<<MAPPED>>> mgr addr %p client addr %p\n ",(vaddr_t)l_addr, (vaddr_t)d_addr); */
	
	cci->desc.owner.addr = (vaddr_t)d_addr;

	cci->page = (vaddr_t)l_addr;  //keep local address for later revoke
	cci->parent_spdid = d_spdid;

	// add the cbuf to shared vect here? now we do it in the client.
	// and l_addr and d_addr has been assinged
done:
	return;
err:
	printc("Cbuf mgr: Cannot alias page to client!\n");
	mman_release_page(cos_spd_id(), (vaddr_t)l_addr, 0);
	/* valloc_free(cos_spd_id(), cos_spd_id(), l_addr, 1); */
	valloc_free(cos_spd_id(), cos_spd_id(), (void *)d_addr, 1);
	goto done;
}

void __cbuf_c_delete(struct spd_tmem_info *sti, int cbid);

tmem_item * mgr_get_client_mem(struct spd_tmem_info *sti)
{
	spdid_t s_spdid;
	struct cos_cbuf_item * cci = NULL, *list;
	struct spd_cbvect_range *cbr;

	assert(sti);
	s_spdid = sti->spdid;

	list = &spd_tmem_info_list[s_spdid].tmem_list;

	union cbuf_meta cm;
	/* Go through the allocated cbufs, and see if any are not in use... */
	for (cci = FIRST_LIST(list, next, prev) ; 
	     cci != list ; 
	     cci = FIRST_LIST(cci, next, prev)) {
		cbr = &sti->ci;
		cm.c_0.v = cbr->meta[(cci->desc.cbid & COS_VECT_MASK)].c_0.v;
		/* cm.c_0.v = (u32_t)((sti->ci)[(cci->desc.cbid & COS_VECT_MASK)].val); */
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
	struct spd_cbvect_range *cbr;

	printc("In mark relinquish!\n");
	for(cci = FIRST_LIST(&sti->tmem_list, next, prev);
	    cci != &sti->tmem_list;
	    cci = FIRST_LIST(cci, next, prev)){
		cbr = &sti->ci;
		cm.c_0.v = cbr->meta[(cci->desc.cbid & COS_VECT_MASK)].c_0.v;
		cm.c.flags |= CBUFM_RELINQUISH;
		cbr->meta[(cci->desc.cbid & COS_VECT_MASK)].c_0.v = cm.c_0.v;
		/* cm.c_0.v = (u32_t)((sti->ci->meta)[(cci->desc.cbid & COS_VECT_MASK)].val); */
		/* cm.c.flags |= CBUFM_RELINQUISH; */
		/* (sti->ci->meta)[(cci->desc.cbid & COS_VECT_MASK)].val = (void *)cm.c_0.v; */
	}
}

void spd_unmark_relinquish(struct spd_tmem_info *sti)
{
	struct cos_cbuf_item *cci;
	union cbuf_meta cm;
	struct spd_cbvect_range *cbr;

	for(cci = FIRST_LIST(&sti->tmem_list, next, prev);
	    cci != &sti->tmem_list;
	    cci = FIRST_LIST(cci, next, prev)){
		cbr = &sti->ci;
		cm.c_0.v = cbr->meta[(cci->desc.cbid & COS_VECT_MASK)].c_0.v;
		cm.c.flags &= ~CBUFM_RELINQUISH;
		cbr->meta[(cci->desc.cbid & COS_VECT_MASK)].c_0.v = cm.c_0.v;
		/* cm.c_0.v = (u32_t)((sti->ci)[(cci->desc.cbid & COS_VECT_MASK)].val); */
		/* cm.c.flags &= ~CBUFM_RELINQUISH; */
		/* (sti->ci)[(cci->desc.cbid & COS_VECT_MASK)].val = (void *)cm.c_0.v; */
	}
}

u32_t 
resolve_dependency(struct spd_tmem_info *sti, int skip_stk)
{
	struct cos_cbuf_item *cci;
	/* union cbuf_meta cm; */
	struct spd_cbvect_range *cbr;
	int conv_cbid;

	cbr = &sti->ci;

	for(cci = FIRST_LIST(&sti->tmem_list, next, prev);
	    cci != &sti->tmem_list && skip_stk > 0; 
	    cci = FIRST_LIST(cci, next, prev), skip_stk--) ;

	if (cci == &sti->tmem_list) return 0;

	conv_cbid = cci->desc.cbid * 2 + 1;

	return (u32_t)((cbr->meta)[(conv_cbid & COS_VECT_MASK)].c_0.th_id);
}

static inline int
__spd_cbvect_add_range(struct spd_tmem_info *sti, long cbuf_id, vaddr_t page)
{
	struct spd_cbvect_range *cbr;

	cbr = malloc(sizeof(struct spd_cbvect_range));
	if (!cbr) return -1;

	cbr->start_id = cbuf_id & ~CBUF_VECT_MASK;
	cbr->end_id = cbr->start_id + CBUF_VECT_PAGE_BASE - 1;
	cbr->meta = (union cbuf_meta*)page;

	ADD_LIST(&sti->ci, cbr, next, prev);

	return 0;
}

static inline union cbuf_meta *
__spd_cbvect_lookup_range(struct spd_tmem_info *sti, long cbuf_id)
{
	struct spd_cbvect_range *cbr;

	for (cbr = FIRST_LIST(&sti->ci, next, prev) ; 
	     cbr != &sti->ci ; 
	     cbr = FIRST_LIST(cbr, next, prev)) {
		if (cbuf_id >= cbr->start_id && cbuf_id <= cbr->end_id) {
			return &cbr->meta[(cbuf_id - cbr->start_id - 1)];
		}
	}
	return NULL;
}

vaddr_t
cbuf_c_register(spdid_t spdid, long cbid)
{
	struct spd_tmem_info *sti;
	vaddr_t p, mgr_addr;

	printc("\nREGISTERED!!!\n");
	sti = get_spd_info(spdid);
	
	mgr_addr = (vaddr_t)alloc_page();
	p = (vaddr_t)valloc_alloc(cos_spd_id(), spdid, 1);
	if (p !=
	    (mman_alias_page(cos_spd_id(), mgr_addr, spdid, p))) {
		printc("mapped faied p is %p\n",(void *)p);
		valloc_free(cos_spd_id(), spdid, p, 1);
		return -1;
	}
	sti->managed = 1;
	/* __spd_cbvect_add_range(sti, cbid, (struct cbuf_vect_intern_struct *)mgr_addr); */
	__spd_cbvect_add_range(sti, cbid, mgr_addr);

	return p;
}

int
cbuf_c_create(spdid_t spdid, int size, long cbid)
{
	int ret = -1;
	void *v;
	struct spd_tmem_info *sti;
	struct cos_cbuf_item *cbuf_item;
	struct cb_desc *d;

	union cbuf_meta *mc = NULL;

	printc("cbuf_c_create is called here!!\n");

	TAKE();

	sti = get_spd_info(spdid);
	
	/* Make sure we have access to the component shared page */
	assert(SPD_IS_MANAGED(sti));
	assert(cbid >= 0);

	if (cbid) {
		 // vector should already exist
		v = cos_map_lookup(&cb_ids, cbid);
		if (unlikely(v != (void *)spdid)){
			goto err;
		}
	}
	else {
		cbid = cos_map_add(&cb_ids, (void *)spdid);
		if ((mc = __spd_cbvect_lookup_range(sti, cbid)) == NULL){
			RELEASE();
			return cbid*-1;	
		} 
	}
	/* printc("delete cbid is %ld\n",cbid); */
	cos_map_del(&cb_ids, cbid);

	/* call trasient memory grant! */
	cbuf_item = tmem_grant(sti);

	assert(cbuf_item);

	d = &cbuf_item->desc;
	d->principal  = cos_get_thd_id();
	d->obj_sz     = size;
	d->owner.spd  = sti->spdid;
	d->owner.cbd  = d;
	INIT_LIST(&d->owner, next, prev);

	cbid = cos_map_add(&cb_ids, d);
	/* printc("new cbid is %ld\n",cbid); */
	ret = d->cbid = cbid;


	mc = __spd_cbvect_lookup_range(sti, cbid);
	assert(mc);

	/* printc("get mc here from mgr %p\n",mc); */
	//FIXME: RACE here!
	// add cbuf_meta.c_0.v
	mc->c.ptr = d->owner.addr >> PAGE_ORDER;
	mc->c.obj_sz = size >> 6;
	
	/* mc = __spd_cbvect_lookup_range(sti, cbid); */
	/* printc("mc->c_0.v is %p\n", mc->c_0.v); */
	/* printc("ptr is  %p\n", mc->c.ptr); */
	/* printc("size is  %p\n", mc->c.obj_sz); */
	/* printc("size is  %d\n", mc->c.obj_sz << 6); */

	// add thd_id
	mc->c_0.th_id = cos_get_thd_id();
	printc("\nthread id is added: %ld\n",mc->c_0.th_id);

done:
	RELEASE();
	return ret;
err:
	ret = -1;
	goto done;
}

void __cbuf_c_delete(struct spd_tmem_info *sti, int cbid)
{
	struct cb_desc *d;
	struct cb_mapping *m;
	struct spd_cbvect_range *cbr;	
	struct spd_tmem_info *map_sti;
	cbr = &sti->ci;	

	printc("deleting, before: %d\n",cbr->meta[cbid & COS_VECT_MASK].c_0.v);
	cbr->meta[cbid & COS_VECT_MASK].c_0.v = (u32_t)COS_VECT_INIT_VAL;
	cbr->meta[cbid & COS_VECT_MASK].c_0.th_id = (u32_t)COS_VECT_INIT_VAL;
	printc("deleting, after: %d\n",cbr->meta[cbid & COS_VECT_MASK].c_0.v);

	d = cos_map_lookup(&cb_ids, cbid);
	if (!d) goto done;
	/* should be conditional on the principal??? */
	if (d->owner.spd != sti->spdid) goto done;
	cos_map_del(&cb_ids, cbid);
	/* mapping model will release all child mappings */
	DOUT("Releasing cbuf\n");
	mman_release_page(cos_spd_id(), (vaddr_t)d->addr, 0);
	valloc_free(cos_spd_id(), sti->spdid, (void *)(d->owner.addr), 1);// used to be done in client

	m = FIRST_LIST(&d->owner, next, prev);
	while (m != &d->owner) {
		/* remove from the vector as well! */
		map_sti = get_spd_info(m->spd);
		cbr = &map_sti->ci;			
		// TODO: check if mapped cbuf is being used?
		cbr->meta[cbid & COS_VECT_MASK].c_0.v = (u32_t)COS_VECT_INIT_VAL;
		cbr->meta[cbid & COS_VECT_MASK].c_0.th_id = (u32_t)COS_VECT_INIT_VAL;
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
	printc("start returning!\n");
	sti = get_spd_info(spdid);
	assert(sti);

	printc("Return of s_spdid is: %d from thd: %d\n", spdid, cos_get_thd_id());

	__cbuf_c_delete(sti, cbid);
	return_tmem(sti);
//	printc("finished return!");
	RELEASE();
}

void *
cbuf_c_retrieve(spdid_t spdid, int cbid, int len)
{
	void *ret = NULL;
	char *l_addr, *d_addr;
	struct spd_tmem_info *sti;
	struct cb_desc *d;
	struct cb_mapping *m;

	TAKE();

	d = cos_map_lookup(&cb_ids, cbid);
	/* sanity and access checks */
	if (!d || d->obj_sz < len) goto done;
#ifdef PRINCIPAL_CHECKS
	if (d->principal != cos_get_thd_id()) goto done;
#endif
	/* printc("info: thd_id %d obj_size %d addr %p\n", d->principal, d->obj_sz, d->addr); */
	m = malloc(sizeof(struct cb_mapping));
	if (!m) goto done;

	INIT_LIST(m, next, prev);

	d_addr = valloc_alloc(cos_spd_id(), spdid, 1);
	l_addr = d->addr;  //cbuf_item addr, initialized in cos_init()

	assert(d_addr && l_addr);

	/* if (!mman_alias_page(cos_spd_id(), (vaddr_t)d->addr, spdid, (vaddr_t)page)) goto err; */
	if (unlikely(!mman_alias_page(cos_spd_id(), (vaddr_t)l_addr, spdid, (vaddr_t)d_addr)))
		goto err;
	/* printc("<<<MAPPED>>> mgr addr %p client addr %p\n ",(vaddr_t)l_addr, (vaddr_t)d_addr); */

	m->cbd  = d;
	m->spd  = spdid;
	m->addr = (vaddr_t)d_addr;

	ADD_LIST(&d->owner, m, next, prev);
	/* ret = cbid; */
	ret = (void *)d_addr;
done:
	RELEASE();
	return ret;
err:
	valloc_free(cos_spd_id(), spdid, d_addr, 1);
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
		INIT_LIST(&spd_tmem_info_list[i].ci, next, prev);
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
			DOUT("<cbuf_mgr>: ERROR, could not allocate page for cbuf\n"); 
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
		spd_tmem_info_list[spdid].ci.meta = NULL; 
		spd_tmem_info_list[spdid].managed = 1;

    
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

 
/* static inline void */
/* map_cbuf_vect_info(spdid_t spdid) */
/* { */
/* 	/\* spdid_t s; *\/ */
/* 	/\* int i; *\/ */
/* 	/\* int found = 0; *\/ */
/* 	void *hp; */

/* 	assert(spdid < MAX_NUM_SPDS); */

/* 	/\* for (i = 0; i < MAX_NUM_SPDS; i++) { *\/ */
/* 	/\* 	s = cinfo_get_spdid(i); *\/ */
/* 	/\* 	if(!s) {  *\/ */
/* 	/\* 		printc("Unable to map compoents cinfo page!\n"); *\/ */
/* 	/\* 		BUG(); *\/ */
/* 	/\* 	} *\/ */
            
/* 	/\* 	if (s == spdid) { *\/ */
/* 	/\* 		found = 1; *\/ */
/* 	/\* 		break; *\/ */
/* 	/\* 	} *\/ */
/* 	/\* }  *\/ */
    
/* 	/\* if(!found){ *\/ */
/* 	/\* 	DOUT("Could not find cinfo for spdid: %d\n", spdid); *\/ */
/* 	/\* 	BUG(); *\/ */
/* 	/\* } *\/ */
    
/* 	/\* hp = cos_get_vas_page(); *\/ */
/* 	hp = valloc_alloc(cos_spd_id(), cos_spd_id(), 1); */

/* 	// map cbuf info into manager here */

/* 	/\* if(cbuf_info_map(cos_spd_id(), (vaddr_t)hp, s)){ *\/ */
/* 	/\* 	DOUT("Could not map cinfo page for %d\n", spdid); *\/ */
/* 	/\* 	BUG(); *\/ */
/* 	/\* } *\/ */
/* 	spd_tmem_info_list[spdid].ci.meta = hp; */
/* 	spd_tmem_info_list[spdid].managed = 1; */

/* 	/\* DOUT("mapped -- id: %ld, hp:%x, sp:%x\n", *\/ */
/* 	/\*      spd_stk_info_list[spdid].ci->cos_this_spd_id,  *\/ */
/* 	/\*      (unsigned int)spd_stk_info_list[spdid].ci->cos_heap_ptr, *\/ */
/* 	/\*      (unsigned int)spd_stk_info_list[spdid].ci->cos_stacks.freelists[0].freelist); *\/ */
/* } */


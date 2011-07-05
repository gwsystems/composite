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

/* typedef enum { */
/* 	CBUFM_LARGE = 1, */
/* 	CBUFM_RO    = 1<<1, */
/* 	CBUFM_GRANT = 1<<2, */
/* 	CBUFM_IN_USE = 1<<3, */
/* 	CBUFM_RELINQUISH = 1<<4 */
/* } cbufm_flags_t; */


/* struct cb_desc; */
/* struct cb_mapping { */
/* 	spdid_t spd; */
/* 	vaddr_t addr;		/\* other component's map address *\/ */
/* 	struct cb_mapping *next, *prev; */
/* 	struct cb_desc *cbd; */
/* }; */

/* /\* Data we wish to track for every cbuf *\/ */
/* struct cb_desc { */
/* 	u16_t principal;	/\* principal that owns the memory *\/ */
/* 	int cbid;		/\* cbuf id *\/ */
/* 	int obj_sz; */
/* 	void *addr; 	/\* local map address *\/ */
/* 	struct cb_mapping owner; */
/* }; */


/* struct cos_cbuf_item { */
/* 	struct cos_cbuf_item *next, *prev; */
/* 	struct cos_cbuf_item *free_next; */
/* 	spdid_t parent_spdid;	 */
/* 	struct cb_desc *desc_ptr; */
/* }; */



COS_MAP_CREATE_STATIC(cb_ids);

//  all cbufs that created for this component

/* cbuf_freelist_add(struct spd_stk_info *ssi, struct cos_stk_item *csi) */
/* { */
/* 	/\* Should either belong to this spd, or not to another (we */
/* 	 * don't want it mapped into two components) *\/ */
/* 	assert(csi->parent_spdid == ssi->spdid || EMPTY_LIST(csi, next, prev)); */
/* 	assert(ssi->ci); */

/* 	/\* FIXME: race *\/ */
/* 	csi->stk->next = (struct cos_stk*)ssi->ci->cos_stacks.freelists[0].freelist; */
/* 	ssi->ci->cos_stacks.freelists[0].freelist = D_COS_STK_ADDR(csi->d_addr); */

/* 	return 0; */
/* } */


void mgr_map_client_mem(tmem_item *csi, struct spd_stk_info *info)
{
	vaddr_t d_addr, stk_addr;
	spdid_t d_spdid;
	assert(info && csi);
	assert(EMPTY_LIST(csi, next, prev));

	d_spdid = info->spdid;
	
	d_addr = (vaddr_t)valloc_alloc(cos_spd_id(), d_spdid, 1);
	
//	DOUT("Setting flags and assigning flags\n");
	csi->flags = 0xDEADBEEF;
	csi->next = (void *)0xDEADBEEF;
	stk_addr = (vaddr_t)(csi->desc_ptr->addr);
	if(unlikely(d_addr != mman_alias_page(cos_spd_id(), stk_addr, d_spdid, d_addr))){
		printc("<stkmgr>: Unable to map stack into component");
		BUG();
	}
//	DOUT("Mapped page\n");
	csi->d_addr = d_addr;
	csi->parent_spdid = d_spdid;
    
	// Add stack to allocated stack array
//	DOUT("Adding to local spdid stk list\n");
	ADD_LIST(&info->tmem_list, csi, next, prev);
	info->num_allocated++;
	if (info->num_allocated == 1) empty_comps--;
	if (info->num_allocated > info->num_desired) over_quota_total++;
	assert(info->num_allocated == tmem_num_alloc_stks(info->spdid));

//	cbuf_freelist_add(info, csi);
	return;

}

tmem_item * mgr_get_client_mem(struct spd_stk_info *ssi)
{
	return NULL;
}

void spd_mark_relinquish(spdid_t spdid)
{
	struct cos_cbuf_item *cbuf_item;

	/* for(cbuf_item = FIRST_LIST(&spd_stk_info_list[spdid].tmem_list, next, prev); */
	/*     cbuf_item != &spd_stk_info_list[spdid].tmem_list;  */
	/*     cbuf_item = FIRST_LIST(cbuf_item, next, prev)){ */
	/* 	cbuf_item->flags |= RELINQUISH; */
	/* } */

}

void spd_unmark_relinquish(struct spd_stk_info *ssi)
{

}

u32_t resolve_dependency(struct spd_stk_info *ssi, int skip_stk)
{
	return 0;
}

int get_cbuf_id()
{
	return cos_spd_id();
}

vaddr_t
cbuf_c_register(spdid_t spdid)
{
	struct spd_stk_info *ssi;
	vaddr_t p, mgr_addr;

	ssi = get_spd_info(spdid);
	
	mgr_addr = (vaddr_t)alloc_page();

	p = (vaddr_t)valloc_alloc(cos_spd_id(), spdid, 1);

	if (p !=
	    (mman_alias_page(cos_spd_id(), mgr_addr, spdid, p))) {
		printc("mapped faied p is %p\n",(void *)p);
		return -1;
	}

	printc("p here %p\n",(void *)p);

	ssi->ci = (struct cbuf_vect_t *)mgr_addr;

	return p;

}


static inline void
map_cbuf_vect_info(spdid_t spdid)
{
	spdid_t s;
	int i;
	int found = 0;
	void *hp;

	assert(spdid < MAX_NUM_SPDS);

	/* for (i = 0; i < MAX_NUM_SPDS; i++) { */
	/* 	s = cinfo_get_spdid(i); */
	/* 	if(!s) {  */
	/* 		printc("Unable to map compoents cinfo page!\n"); */
	/* 		BUG(); */
	/* 	} */
            
	/* 	if (s == spdid) { */
	/* 		found = 1; */
	/* 		break; */
	/* 	} */
	/* }  */
    
	/* if(!found){ */
	/* 	DOUT("Could not find cinfo for spdid: %d\n", spdid); */
	/* 	BUG(); */
	/* } */
    
	/* hp = cos_get_vas_page(); */
	hp = valloc_alloc(cos_spd_id(), cos_spd_id(), 1);

	// map cbuf info into manager here

	/* if(cbuf_info_map(cos_spd_id(), (vaddr_t)hp, s)){ */
	/* 	DOUT("Could not map cinfo page for %d\n", spdid); */
	/* 	BUG(); */
	/* } */
	spd_stk_info_list[spdid].ci = hp;
	spd_stk_info_list[spdid].managed = 1;

	/* DOUT("mapped -- id: %ld, hp:%x, sp:%x\n", */
	/*      spd_stk_info_list[spdid].ci->cos_this_spd_id,  */
	/*      (unsigned int)spd_stk_info_list[spdid].ci->cos_heap_ptr, */
	/*      (unsigned int)spd_stk_info_list[spdid].ci->cos_stacks.freelists[0].freelist); */
}


int
cbuf_c_create(spdid_t spdid, int size, void *page)
{
	struct cb_desc *d;
	char *h;
	int ret = -1, cbid;

	if (size > PAGE_SIZE) goto done;
	d = malloc(sizeof(struct cb_desc));
	if (!d) goto done;

	struct spd_stk_info *info;

	TAKE();

	info = get_spd_info(spdid);
	
	/* // Make sure we have access to the info page */
	if (!SPD_IS_MANAGED(info)) map_cbuf_vect_info(spdid);
	assert(SPD_IS_MANAGED(info));

//	tmem_grant(info);

	/* TODO: multiple pages cbuf! */
	h = valloc_alloc(cos_spd_id(), cos_spd_id(), 1);
	/* get the page */
	if (!mman_get_page(cos_spd_id(), (vaddr_t)h, 0)) goto err;
	/* ...map it into the requesting component */
	if (!mman_alias_page(cos_spd_id(), (vaddr_t)h, spdid, (vaddr_t)page)) goto err2;

	d->principal  = cos_get_thd_id();
	d->obj_sz     = size;
	d->addr       = h;
	d->owner.spd  = spdid;
	d->owner.addr = (vaddr_t)page;
	d->owner.cbd  = d;
	INIT_LIST(&d->owner, next, prev);
	cbid          = cos_map_add(&cb_ids, d);
	ret = d->cbid = cbid;
done:
	RELEASE();
	return ret;
err2:
	mman_release_page(cos_spd_id(), (vaddr_t)h, 0);
err:
	valloc_free(cos_spd_id(), cos_spd_id(),h, 1);
	goto done;
}

/* 
 * FIXME: 1) reference counting so that components can maintain the
 * buffer if they please, 2) asynchronous (shmmem) notification of cb
 * deallocation.
 */
void
cbuf_c_delete(spdid_t spdid, int cbid)
{
	struct cb_desc *d;
	struct cb_mapping *m;
	
	TAKE();
	d = cos_map_lookup(&cb_ids, cbid);
	if (!d) goto done;
	/* should be conditional on the principal??? */
	if (d->owner.spd != spdid) goto done;
	cos_map_del(&cb_ids, cbid);
	/* mapping model will release all child mappings */
	mman_release_page(cos_spd_id(), (vaddr_t)d->addr, 0);
	m = FIRST_LIST(&d->owner, next, prev);
	while (m != &d->owner) {
		struct cb_mapping *n = FIRST_LIST(m, next, prev);
		REM_LIST(m, next, prev);
		free(m);
		m = n;
	}
	free(d);
done:
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
	/* struct cos_cbuf_item *cbuf_item; */

	memset(spd_stk_info_list, 0, sizeof(struct spd_stk_info) * MAX_NUM_SPDS);
    
	for(i = 0; i < MAX_NUM_SPDS; i++){
		spd_stk_info_list[i].spdid = i;    
		INIT_LIST(&spd_stk_info_list[i].tmem_list, next, prev);
		INIT_LIST(&spd_stk_info_list[i].bthd_list, next, prev);
	}

	free_tmem_list = NULL;

	/* // Initialize our free stack list */
	/* for(i = 0; i < MAX_NUM_STACKS; i++){ */
        
	/* 	// put stk list is some known state */
	/* 	stk_item->stk  = NULL; */
	/* 	INIT_LIST(stk_item, next, prev); */
        
	/* 	// allocate a page */
	/* 	stk_item->hptr = alloc_page(); */
	/* 	if (stk_item->hptr == NULL){ */
	/* 		DOUT("<stk_mgr>: ERROR, could not allocate stack\n");  */
	/* 	} else { */
	/* 		// figure out or location of the top of the stack */
	/* 		stk_item->stk = (struct cos_stk *)D_COS_STK_ADDR((char *)stk_item->hptr); */
	/* 		freelist_add(stk_item); */
	/* 	} */
	/* } */

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
		/* spd_stk_info_list[spdid].ci = hp;  */
		spd_stk_info_list[spdid].ci = NULL; 
		spd_stk_info_list[spdid].managed = 1;

		/* DOUT("mapped -- id: %ld, hp:%x, sp:%x\n", */
		/*      spd_stk_info_list[spdid].ci->cos_this_spd_id,  */
		/*      (unsigned int)spd_stk_info_list[spdid].ci->cos_heap_ptr, */
		/*      (unsigned int)spd_stk_info_list[spdid].ci->cos_stacks.freelists[0].freelist); */
    
		stacks_target += DEFAULT_TARGET_ALLOC;
		spd_stk_info_list[spdid].num_allocated = 0;
		spd_stk_info_list[spdid].num_desired = DEFAULT_TARGET_ALLOC;
		spd_stk_info_list[spdid].num_blocked_thds = 0;
		spd_stk_info_list[spdid].num_waiting_thds = 0;
		spd_stk_info_list[spdid].ss_counter = 0;
		spd_stk_info_list[spdid].ss_max = MAX_NUM_CBUFS;
		empty_comps++;
	}
	over_quota_total = 0;
	over_quota_limit = MAX_NUM_CBUFS;
	/* DOUT("Done mapping components information pages!\n"); */
	/* DOUT("<stkmgr>: init finished\n"); */
	return;

}

void
bin(void)
{
	sched_block(cos_spd_id(), 0);
}


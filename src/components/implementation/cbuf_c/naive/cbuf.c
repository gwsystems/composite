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

COS_MAP_CREATE_STATIC(cb_ids);
/* cos_lock_t l; */
/* #define TAKE() lock_take(&l); */
/* #define RELEASE() lock_release(&l); */


void mgr_map_client_mem(tmem_item *tmi, struct spd_stk_info *info)
{

}

tmem_item * mgr_get_client_mem(struct spd_stk_info *ssi)
{
	return NULL;
}

void spd_mark_relinquish(spdid_t spdid)
{

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
cbuf_c_register(spdid_t spdid, struct cbuf_vect_intern_struct *is)
{
	struct spd_stk_info *ssi;
	vaddr_t p;

	ssi = get_spd_info(spdid);
	
	p = (vaddr_t)valloc_alloc(cos_spd_id(), cos_spd_id(), 1);
	
	return p;

	/* printc("is %p. s_spdid %ld\n",is, spdid);  */
	if (p !=
	    (mman_alias_page(spdid, (vaddr_t)is, cos_spd_id(), p))) {
		printc("mapped faied p is %p\n",p);
		return -1;
	}
	
	ssi->ci = (struct cbuf_vect_intern_struct *)p;
	/* printc("mapped p is %p\n",ssi->ci);  */

	return 0;
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
	TAKE();
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


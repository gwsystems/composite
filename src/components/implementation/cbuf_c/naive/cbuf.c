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


tmem_item * free_mem_in_local_cache(struct spd_tmem_info *sti)
{
	spdid_t s_spdid;
	struct cos_cbuf_item *cci = NULL, *list;
	struct spd_cbvect_range *cbr;
	assert(sti);
	s_spdid = sti->spdid;

	printc("\n Check if in local cache!!!");
	//list = get_spd_info(s_spdid).tmem_list;
	list = &spd_tmem_info_list[s_spdid].tmem_list;
	/* Go through the allocated cbufs, and see if any are not in use... */
	for (cci = FIRST_LIST(list, next, prev) ; 
	     cci != list; 
	     cci = FIRST_LIST(cci, next, prev)) {
		for (cbr = FIRST_LIST(&sti->ci, next, prev) ; 
		     cbr != &sti->ci && cbr->meta != 0; 
		     cbr = FIRST_LIST(cbr, next, prev)) {
			if (cci->desc.cbid >= cbr->start_id && cci->desc.cbid <= cbr->end_id) {
				union cbuf_meta cm;
				cm.c_0.v = cbr->meta[(cci->desc.cbid - cbr->start_id - 1)].c_0.v;
				if (!CBUF_IN_USE(cm.c.flags)) goto done;
			}
		}
	}

	if (cci == list) goto err;
done:
	printc("\n found one!!\n\n");
	return cci;
err:
	printc("\n can not found one!!\n");
	cci = NULL;	
	return cci;
}



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
	valloc_free(cos_spd_id(), d_spdid, (void *)d_addr, 1);
	goto done;
}

int __cbuf_c_delete(struct spd_tmem_info *sti, int cbid, struct cb_desc *d);

tmem_item * mgr_get_client_mem(struct spd_tmem_info *sti)
{
	spdid_t s_spdid;
	struct cb_desc *d;
	struct cos_cbuf_item *cci = NULL, *list;
	struct spd_cbvect_range *cbr;
	assert(sti);
	s_spdid = sti->spdid;

	//list = get_spd_info(s_spdid).tmem_list;
	list = &spd_tmem_info_list[s_spdid].tmem_list;
	/* Go through the allocated cbufs, and see if any are not in use... */
	for (cci = FIRST_LIST(list, next, prev) ; 
	     cci != list ; 
	     cci = FIRST_LIST(cci, next, prev)) {
		for (cbr = FIRST_LIST(&sti->ci, next, prev) ; 
		     cbr != &sti->ci ; 
		     cbr = FIRST_LIST(cbr, next, prev)) {
			if (cci->desc.cbid >= cbr->start_id && cci->desc.cbid <= cbr->end_id) {
				union cbuf_meta cm;

				//printc("::spd %d\n",sti->spdid);
				//printc("cbid is %d\n",cci->desc.cbid);
				cm.c_0.v = cbr->meta[(cci->desc.cbid - cbr->start_id - 1)].c_0.v;
				/* cm.c_0.v = cbr->meta[(cci->desc.cbid & COS_VECT_MASK)].c_0.v; */
				//printc("cm.c_0.v is %p\n",cm.c_0.v);
				//printc("cm.c.flags is %p\n",cm.c.flags);
				/* printc("CBUF_IN_USE is %p\n",CBUF_IN_USE(cm.c.flags)); */
				if (!CBUF_IN_USE(cm.c.flags)) goto out;
			}
		}
	}
out:
	if (cci == list) goto err;
	/* ...and remove it if it is still valid and not in use by owner */
	d = cos_map_lookup(&cb_ids, cci->desc.cbid);
	if(!d) goto err;

	if (__cbuf_c_delete(sti, cci->desc.cbid,d)) goto err;
	cos_map_del(&cb_ids, cci->desc.cbid);

	cci->desc.cbid = 0;
	cci->parent_spdid = 0;
	
	// Clear our memory to prevent leakage
	memset(cci->desc.addr, 0, PAGE_SIZE);
	
	/* printc("Removing from local list\n"); */

	REM_LIST(cci, next, prev);
	/* TODO: move all of this into the tmem generic code just like the ++s */
	sti->num_allocated--;
	if (sti->num_allocated == 0) empty_comps++;
	if (sti->num_allocated >= sti->num_desired) over_quota_total--;
	assert(sti->num_allocated == tmem_num_alloc_stks(s_spdid));

	printc("Leaving get cli mem:: num_allocated %d  num_desired %d\n",sti->num_allocated, sti->num_desired);

done:
	return cci;
err: 
	cci = NULL;
	goto done;
}

void spd_mark_relinquish(struct spd_tmem_info *sti)   // called in mem_grant
{
	struct cos_cbuf_item *cci;
	union cbuf_meta cm;
	struct spd_cbvect_range *cbr;

	printc("thd %d In mark relinquish!\n", cos_get_thd_id());
	/* for each cbuf for a specific spd... */
	for(cci = FIRST_LIST(&sti->tmem_list, next, prev);
	    cci != &sti->tmem_list;
	    cci = FIRST_LIST(cci, next, prev)){
		/* ...for each vector range in the second level cbuf_vects... */
		for (cbr = FIRST_LIST(&sti->ci, next, prev) ; 
		     cbr != &sti->ci ; 
		     cbr = FIRST_LIST(cbr, next, prev)) {
			/* ...find the correct aliased vector page */
			if (cci->desc.cbid >= cbr->start_id && cci->desc.cbid <= cbr->end_id) {
				/* printc("cbid %d spd %d  sti %p cbr->meta %p\n",cci->desc.cbid,sti->spdid,sti, cbr->meta); */
				cm.c_0.v = cbr->meta[(cci->desc.cbid - cbr->start_id - 1)].c_0.v;
				cm.c.flags |= CBUFM_RELINQUISH;
				cbr->meta[(cci->desc.cbid - cbr->start_id - 1)].c_0.v = cm.c_0.v;
				break;
			}
		}
	}
}

void spd_unmark_relinquish(struct spd_tmem_info *sti)
{
	struct cos_cbuf_item *cci;
	union cbuf_meta cm;
	struct spd_cbvect_range *cbr;

	printc("thd %d In unmark relinquish!\n", cos_get_thd_id());

	for(cci = FIRST_LIST(&sti->tmem_list, next, prev);
	    cci != &sti->tmem_list;
	    cci = FIRST_LIST(cci, next, prev)){
		for (cbr = FIRST_LIST(&sti->ci, next, prev) ; 
		     cbr != &sti->ci ; 
		     cbr = FIRST_LIST(cbr, next, prev)) {
			if (cci->desc.cbid >= cbr->start_id && cci->desc.cbid <= cbr->end_id) {
				cm.c_0.v = cbr->meta[(cci->desc.cbid - cbr->start_id - 1)].c_0.v;
				cm.c.flags &= ~CBUFM_RELINQUISH;
				cbr->meta[(cci->desc.cbid - cbr->start_id - 1)].c_0.v = cm.c_0.v;
				break;
			}
		}
	}
}

u32_t 
resolve_dependency(struct spd_tmem_info *sti, int skip_stk)
{
	struct cos_cbuf_item *cci;
	/* union cbuf_meta cm; */
	struct spd_cbvect_range *cbr;
	int conv_cbid;
	u32_t ret = 0;

	/* cbr = &sti->ci; */
	for(cci = FIRST_LIST(&sti->tmem_list, next, prev);
	    cci != &sti->tmem_list && skip_stk > 0; 
	    cci = FIRST_LIST(cci, next, prev), skip_stk--) ;

	if (cci == &sti->tmem_list) goto done;

	/* conv_cbid = cci->desc.cbid * 2 + 1; */
	
	for (cbr = FIRST_LIST(&sti->ci, next, prev) ; 
	     cbr != &sti->ci ; 
	     cbr = FIRST_LIST(cbr, next, prev)) {
		if (cci->desc.cbid >= cbr->start_id && cci->desc.cbid <= cbr->end_id) {
			union cbuf_meta cm;
			
			cm.c_0.v = cbr->meta[(cci->desc.cbid - cbr->start_id - 1)].c_0.v;
			printc("flags is %p \n", cm.c.flags);
			if (!CBUF_IN_USE(cm.c.flags)) {
				printc("thd id set for PIP, and cbuf not in use!\n");
				break;
			}
			ret = (u32_t)cbr->meta[(cci->desc.cbid - cbr->start_id - 1)].c_0.th_id;
			printc("cm.c_0.v is %p \n", cm.c_0.v);
			printc("ret :: %d current thd : %d \n", ret, cos_get_thd_id());
			if (ret == cos_get_thd_id()) 
				goto none;
			else 
				break;
		}
	}
	printc("foo\n");		

	/* return (u32_t)((cbr->meta)[(conv_cbid & COS_VECT_MASK)].c_0.th_id); */
done:
	return ret;
none:
	ret = 0;
	goto done;
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

	/* printc("spd %d  sti %p cbr->meta %p\n",sti->spdid,sti, cbr->meta); */
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

static inline void
__spd_cbvect_clean_val(struct spd_tmem_info *sti, long cbuf_id)
{
	struct spd_cbvect_range *cbr;

	for (cbr = FIRST_LIST(&sti->ci, next, prev) ; 
	     cbr != &sti->ci ; 
	     cbr = FIRST_LIST(cbr, next, prev)) {
		if (cbuf_id >= cbr->start_id && cbuf_id <= cbr->end_id) {
			cbr->meta[(cbuf_id - cbr->start_id - 1)].c_0.v = (u32_t)COS_VECT_INIT_VAL;
			cbr->meta[(cbuf_id - cbr->start_id - 1)].c_0.th_id = (u32_t)COS_VECT_INIT_VAL;
			break;
		}
	}
	return;
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
	/* printc("passed cbid is %ld\n",cbid); */
	TAKE();


	sti = get_spd_info(spdid);
	/* printc("sti when created %p\n", sti); */
	
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

	/* printc("... cbid is %ld\n",cbid); */

	cos_map_del(&cb_ids, cbid);

	/* call trasient memory grant! */
	cbuf_item = tmem_grant(sti);

	/* printc("cbuf_item->desc.cbid is %d \n",cbuf_item->desc.cbid); */
	assert(cbuf_item);

	d = &cbuf_item->desc;
	d->principal  = cos_get_thd_id();
	d->obj_sz     = size;
	d->owner.spd  = sti->spdid;
	d->owner.cbd  = d;
	INIT_LIST(&d->owner, next, prev);

	if(!cbuf_item->desc.cbid){
		cbid = cos_map_add(&cb_ids, d);   // use new cbuf
	}
	else{
		cbid = cbuf_item->desc.cbid;  // use a local cached one
	}
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
	/* printc("\nthread id is added: %ld\n",mc->c_0.th_id); */

done:
	RELEASE();
	return ret;
err:
	ret = -1;
	goto done;
}


int __cbuf_c_delete(struct spd_tmem_info *sti, int cbid, struct cb_desc *d)
{
	struct cb_mapping *m;
	struct spd_cbvect_range *cbr;	
	struct spd_tmem_info *map_sti;
	int ret = -1;
	printc("cbid is %d\n",cbid);

	mman_revoke_page(cos_spd_id(), (vaddr_t)d->addr, 0);  // remove all mapped children

	m = FIRST_LIST(&d->owner, next, prev);
	while (m != &d->owner) {
		/* remove from the vector in all mapped spds as well! */
		map_sti = get_spd_info(m->spd);
		/* printc("Clean val in spd %d\n", map_sti->spdid); */
		/* printc("Clean: cbid  %d\n",cbid); */
		__spd_cbvect_clean_val(map_sti, cbid);

		valloc_free(cos_spd_id(), m->spd, (void *)(m->addr), 1);
		struct cb_mapping *n = FIRST_LIST(m, next, prev);
		REM_LIST(m, next, prev);
		free(m);
		m = n;
	}
	__spd_cbvect_clean_val(sti, cbid);
	//valloc_free(cos_spd_id(), sti->spdid, (void *)(d->owner.addr), 1);
	ret = 0;
	printc("thd:: %d clean val and ready to return \n", cos_get_thd_id());
err:
	return ret;
}

/* 
 * To release all mapping relation
 */
int
cbuf_c_del_elig(spdid_t spdid, int cbid)
{
	struct cb_desc *d;
	struct spd_tmem_info *sti;
	int ret = 0;  // 0 means the page will stay

	TAKE();
	/* printc("start cleanning cbid :: %d!\n", cbid); */
	sti = get_spd_info(spdid);
	assert(sti);

	/* if (sti->num_blocked_thds) goto err; */
	d = cos_map_lookup(&cb_ids, cbid);
	if (!d) goto err;

	/* should be conditional on the principal??? */
	if (d->owner.spd != sti->spdid) goto err;

	/* mapping model will release all child mappings */
	DOUT("Releasing cbuf\n");

	__cbuf_c_delete(sti, cbid, d);

	if (sti->num_desired >= sti->num_allocated && !sti->num_glb_blocked) {
		printc("Reestablish mapping here!\n");
		mman_alias_page(cos_spd_id(), (vaddr_t)d->addr, sti->spdid, (vaddr_t)d->owner.addr);
		spd_unmark_relinquish(sti);
		ret = 0;
	}
	else
	{
		ret = -1;  
	}
done:
	/* printc("del_elig is verified here and ret :: %d\n", ret); */
	RELEASE();	
	return ret;
err:
	ret = -1;
	goto done;
}


/* 
 * FIXME: 1) reference counting so that components can maintain the
 * buffer if they please, 2) asynchronous (shmmem) notification of cb
 * deallocation.
 */
int
cbuf_c_delete(spdid_t spdid, int cbid, int flag)
{
	struct cb_desc *d;
	struct spd_tmem_info *sti;
	int ret = -1;  // -1 means not really removing from spd and gives back to mgr

	TAKE();

	sti = get_spd_info(spdid);
	assert(sti);

	/* if (sti->num_blocked_thds) goto err; */
	d = cos_map_lookup(&cb_ids, cbid);
	if (!d) goto err;

	/* should be conditional on the principal??? */
	if (d->owner.spd != sti->spdid) goto err;

	/* mapping model will release all child mappings */
	DOUT("Releasing cbuf\n");

	/* printc(" ::: cbid  %d\n",cbid); */

	/* __cbuf_c_delete(sti, cbid, d); */
	if (flag == 1){
		/* printc("start returning!\n"); */
		/* printc("Return of s_spdid is: %d from thd: %d\n", spdid, cos_get_thd_id()); */
		/* cos_map_del(&cb_ids, cbid); */
		valloc_free(cos_spd_id(), sti->spdid, (void *)(d->owner.addr), 1);
		return_tmem(sti);
	}

	tmem_spd_wake_threads(sti);
	assert(!SPD_HAS_BLK_THD(sti));

	if (sti->num_desired >= sti->num_allocated)
		spd_unmark_relinquish(sti);

	printc("thd %d finished return!\n", cos_get_thd_id());
err:
	RELEASE();
	return ret;
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
	/* printc("0-----cbid %d\n",cbid); */
	d = cos_map_lookup(&cb_ids, cbid);
	/* printc("d--%p--\n",d); */
	/* sanity and access checks */
	if (!d || d->obj_sz < len) goto done;
	/* printc("1-----\n"); */
#ifdef PRINCIPAL_CHECKS
	if (d->principal != cos_get_thd_id()) goto done;
#endif
	/* printc("info: thd_id %d obj_size %d addr %p\n", d->principal, d->obj_sz, d->addr); */
	m = malloc(sizeof(struct cb_mapping));
	if (!m) goto done;

	INIT_LIST(m, next, prev);

	d_addr = valloc_alloc(cos_spd_id(), spdid, 1);
	l_addr = d->addr;  //cbuf_item addr, initialized in cos_init()
	/* l_addr = d->owner.addr;  // mapped from owner */

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
	printc("CBUFMgr: %ld in spd %d cbuf mgr running.....\n", cos_get_thd_id(), cos_spd_id());
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

 
void 
cbufmgr_buf_report(void)
{
	tmem_report();
}


int
cbufmgr_set_suspension_limit(spdid_t cid, int max)
{
	return tmem_set_suspension_limit(cid, max);
}

int
cbufmgr_detect_suspension(spdid_t cid, int reset)
{
	return tmem_detect_suspension(cid, reset);
}

int
cbufmgr_set_over_quota_limit(int limit)
{
	return tmem_set_over_quota_limit(limit);
}

int
cbufmgr_get_allocated(spdid_t cid)
{
	return tmem_get_allocated(cid);
}

int
cbufmgr_spd_concurrency_estimate(spdid_t spdid)
{
	return tmem_spd_concurrency_estimate(spdid);
}

unsigned long
cbufmgr_thd_blk_time(unsigned short int tid, spdid_t spdid, int reset)
{
	return tmem_get_thd_blk_time(tid, spdid, reset);
}

int
cbufmgr_thd_blk_cnt(unsigned short int tid, spdid_t spdid, int reset)
{
	return tmem_get_thd_blk_cnt(tid, spdid, reset);
}

void
cbufmgr_spd_meas_reset(void)
{
	tmem_spd_meas_reset();
}

int 
cbufmgr_set_concurrency(spdid_t spdid, int concur_lvl, int remove_spare)
{
	return tmem_set_concurrency(spdid, concur_lvl, remove_spare);
}

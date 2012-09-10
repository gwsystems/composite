/**
 * Copyright 2010 by The George Washington University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Authors and history: 
 * Gabriel Parmer, gparmer@gwu.edu, 2010 -- initial version
 * Qi Wang and Jiguo Song, 2010-12 -- tmem
 * Gabriel Parmer, gparmer@gwu.edu, 2012 -- persistent cbufs
 */

#include <cos_component.h>
#include <sched.h>
#include <cos_synchronization.h>
#include <print.h>
#include <cos_alloc.h>
#include <cos_map.h>
#include <cos_list.h>
#include <cos_debug.h>

#include <cinfo.h>

#include <mem_mgr_large.h>
#include <valloc.h>
#include <mem_pool.h>

#include <tmem.h>
#include <cbuf_c.h>

#define DEFAULT_TARGET_ALLOC MAX_NUM_MEM 

COS_MAP_CREATE_STATIC(cb_ids);

#define CBUF_OBJ_SZ_SHIFT 6
#define CB_IDX(id, cbr) (id - cbr->start_id - 1)

struct cos_cbuf_item *alloc_item_data_struct(void *l_addr) 
{
	struct cos_cbuf_item *cci;
	cci = malloc(sizeof(struct cos_cbuf_item));
	if (!cci) BUG();

	memset(cci, 0, sizeof(struct cos_cbuf_item));
	INIT_LIST(cci, next, prev);
	cci->desc.addr      = l_addr;
	cci->desc.cbid      = 0;
	cci->desc.sz        = 0;

	return cci;
}

void free_item_data_struct(struct cos_cbuf_item *tmi) 
{
	free(tmi);
}

struct cos_cbuf_item *free_mem_in_local_cache(struct spd_tmem_info *sti)
{
	spdid_t s_spdid;
	struct cos_cbuf_item *cci = NULL, *list;

	assert(sti);
	s_spdid = sti->spdid;

	DOUT("\n Check if in local cache!!!");
	list = &spd_tmem_info_list[s_spdid].tmem_list;
	/* Go through the allocated cbufs, and see if any are not in use... */
	for (cci = FIRST_LIST(list, next, prev) ; 
	     cci != list; 
	     cci = FIRST_LIST(cci, next, prev)) {
		struct cbuf_meta cm;
		cm.nfo.v = cci->desc.owner.meta->nfo.v;
		if (!CBUF_IN_USE(cm.nfo.c.flags)) goto done;
	}

	if (cci == list) goto err;
done:
	return cci;
err:
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
	cci->desc.owner.addr = (vaddr_t)d_addr;
	cci->parent_spdid = d_spdid;
	assert(cci->desc.cbid == 0);
	// add the cbuf to shared vect here? now we do it in the client.
	// and l_addr and d_addr has been assinged
done:
	return;
err:
	DOUT("Cbuf mgr: Cannot alias page to client!\n");
	mman_release_page(cos_spd_id(), (vaddr_t)l_addr, 0);
	/* valloc_free(cos_spd_id(), cos_spd_id(), l_addr, 1); */
	valloc_free(cos_spd_id(), d_spdid, (void *)d_addr, 1);
	goto done;
}

int __cbuf_c_delete(struct spd_tmem_info *sti, int cbid, struct cb_desc *d);

static void
mgr_remove_client_mem(struct spd_tmem_info *sti, struct cos_cbuf_item *cci)
{
	__cbuf_c_delete(sti, cci->desc.cbid, &cci->desc);
	cos_map_del(&cb_ids, cci->desc.cbid);

	cci->desc.cbid    = 0;
	cci->parent_spdid = 0;

	// Clear our memory to prevent leakage
	memset(cci->desc.addr, 0, PAGE_SIZE);
	/* printc("Removing from local list\n"); */

	REM_LIST(cci, next, prev);

	/* TODO: move all of this into the tmem generic code just like the ++s */
	sti->num_allocated--;
	if (sti->num_allocated == 0) empty_comps++;

	if (sti->num_allocated >= sti->num_desired) over_quota_total--;
	assert(sti->num_allocated == tmem_num_alloc_tmems(sti->spdid));
}

struct cos_cbuf_item *mgr_get_client_mem(struct spd_tmem_info *sti)
{
	spdid_t s_spdid;
	/* struct cb_desc *d; */
	struct cos_cbuf_item *cci = NULL, *list;
	assert(sti);
	s_spdid = sti->spdid;

	list = &spd_tmem_info_list[s_spdid].tmem_list;

	for (cci = FIRST_LIST(list, next, prev) ; 
	     cci != list ; 
	     cci = FIRST_LIST(cci, next, prev)) {
		struct cbuf_meta cm;
		cm.nfo.v = cci->desc.owner.meta->nfo.v;
		if (!CBUF_IN_USE(cm.nfo.c.flags)) break;
	}

	if (cci == list) goto err;
	assert(&cci->desc == cos_map_lookup(&cb_ids, cci->desc.cbid));
	
	mgr_remove_client_mem(sti, cci);

	DOUT("spd: %d Leaving get cli mem:: num_allocated %d  num_desired %d\n",s_spdid, sti->num_allocated, sti->num_desired);

done:
	return cci;
err: 
	cci = NULL;
	goto done;
}

int
resolve_dependency(struct spd_tmem_info *sti, int skip_cbuf)
{
	struct cos_cbuf_item *cci;
	struct cbuf_meta cm;
	int ret = -1;
	/* DOUT("skip_cbuf is %d\n",skip_cbuf); */

	for(cci = FIRST_LIST(&sti->tmem_list, next, prev);
	    cci != &sti->tmem_list && skip_cbuf > 0; 
	    cci = FIRST_LIST(cci, next, prev), skip_cbuf--) ;

	if (cci == &sti->tmem_list) goto done;

	cm.nfo.v = cci->desc.owner.meta->nfo.v;			

	ret = (u32_t)cci->desc.owner.meta->owner_nfo.thdid;

	if (!CBUF_IN_USE(cm.nfo.c.flags)) goto cache;
	if (ret == cos_get_thd_id()){
		DOUT("Try to depend on itself ....\n");
		goto self;
	}

done:
	return ret;
cache:
	ret = -2;
	goto done;
self:
	ret = 0;
	goto done;
}

void mgr_clear_touched_flag(struct spd_tmem_info *sti)
{
	struct cos_cbuf_item *cci;
	struct cbuf_meta *cm = NULL;

	for (cci = FIRST_LIST(&sti->tmem_list, next, prev) ; 
	     cci != &sti->tmem_list ; 
	     cci = FIRST_LIST(cci, next, prev)) {
		cm = cci->desc.owner.meta;
		if (!CBUF_IN_USE(cm->nfo.c.flags)) {
			cm->nfo.c.flags &= ~CBUFM_TOUCHED;
		} else {
			assert(cm->nfo.c.flags & CBUFM_TOUCHED);
		}
	}
	return;
}

static inline int
__spd_cbvect_add_range(struct spd_tmem_info *sti, long cbuf_id, vaddr_t page)
{
	struct spd_cbvect_range *cbr;

	cbr = malloc(sizeof(struct spd_cbvect_range));
	if (!cbr) return -1;

	cbr->start_id = (cbuf_id - 1) & ~CVECT_MASK;
	cbr->end_id = cbr->start_id + CVECT_BASE - 1;
	cbr->meta = (struct cbuf_meta*)page;

	/* DOUT("spd %d  sti %p cbr->meta %p\n",sti->spdid,sti, cbr->meta); */
	ADD_LIST(&sti->ci, cbr, next, prev);

	/* DOUT("range is added here:: startid %ld endid %ld\n", cbr->start_id, cbr->end_id); */

	return 0;
}

static inline struct cbuf_meta *
__spd_cbvect_lookup_range(struct spd_tmem_info *sti, long cbuf_id)
{
	struct spd_cbvect_range *cbr;

	for (cbr = FIRST_LIST(&sti->ci, next, prev) ; 
	     cbr != &sti->ci ; 
	     cbr = FIRST_LIST(cbr, next, prev)) {
		if (cbuf_id >= cbr->start_id && cbuf_id <= cbr->end_id) {
			return &cbr->meta[CB_IDX(cbuf_id, cbr)];
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
			cbr->meta[CB_IDX(cbuf_id, cbr)].nfo.v = (u32_t)COS_VECT_INIT_VAL;
			cbr->meta[CB_IDX(cbuf_id, cbr)].owner_nfo.thdid = (u32_t)COS_VECT_INIT_VAL;
			break;
		}
	}
	return;
}


static inline vaddr_t
__spd_cbvect_retrieve_page(struct spd_tmem_info *sti, long cbuf_id)
{
	struct spd_cbvect_range *cbr;

	for (cbr = FIRST_LIST(&sti->ci, next, prev) ; 
	     cbr != &sti->ci ; 
	     cbr = FIRST_LIST(cbr, next, prev)) {
		if (cbuf_id >= cbr->start_id && cbuf_id <= cbr->end_id) {
			return (vaddr_t)cbr->meta;
		}
	}
	return 0;
}

vaddr_t
cbuf_c_register(spdid_t spdid, long cbid)
{
	struct spd_tmem_info *sti;
	vaddr_t p, mgr_addr;

	TAKE();
	sti = get_spd_info(spdid);
	
	mgr_addr = (vaddr_t)alloc_page();
	p = (vaddr_t)valloc_alloc(cos_spd_id(), spdid, 1);
	if (p != (mman_alias_page(cos_spd_id(), mgr_addr, spdid, p))) {
		valloc_free(cos_spd_id(), spdid, (void *)p, 1);
		RELEASE();
		return -1;
	}
	sti->managed = 1;
	__spd_cbvect_add_range(sti, cbid, mgr_addr);
	RELEASE();

	return p;
}

extern struct cos_cbuf_item *cbufp_grant(struct spd_tmem_info *sti, int size);

int
cbuf_c_create(spdid_t spdid, int size, long cbid, int tmem)
{
	int ret = -1;
	void *v;
	struct spd_tmem_info *sti;
	struct cos_cbuf_item *cbuf_item;
	struct cb_desc *d;
	struct cbuf_meta *mc = NULL;

	TAKE();
	sti = get_spd_info(spdid);
	/* Make sure we have access to the component shared page */
	assert(SPD_IS_MANAGED(sti));
	assert(cbid >= 0);

	/* 
	 * This is ugly:
	 * 
	 * If we can't find the cbuf id in the meta structure, then we
	 * know we have to let the client extend the cbuf vector.  We
	 * allocate a cbuf id, and set its entry to the spdid that we
	 * are allocating it to.  When this function is called again
	 * with the cbid as the argument, we verify that the entry is
	 * still the spdid, then we actually add the real
	 * cos_cbuf_item to the structure instead of the spdid.
	 */
	if (cbid) {
		 // vector should already exist
		v = cos_map_lookup(&cb_ids, cbid);
		if (unlikely((spdid_t)(int)v != spdid)) goto err;
 	} else {
		cbid = cos_map_add(&cb_ids, (void *)(unsigned long)spdid);
		if ((mc = __spd_cbvect_lookup_range(sti, cbid)) == NULL){
			RELEASE();
			return cbid*-1;	
		} 
	}
	cos_map_del(&cb_ids, cbid);
	/* 
	 * We rely on the FIFO properties of cbid allocation with the
	 * cos_map here to ensure that the new cbuf_item has the same
	 * cbid as the entry we created above.
	 */
	if (tmem) cbuf_item = tmem_grant(sti);
	else      cbuf_item = cbufp_grant(sti, size);
	assert(cbuf_item);
	d                   = &cbuf_item->desc;
	d->sz               = PAGE_SIZE;
	d->owner.spd        = sti->spdid;
	d->owner.cbd        = d;
	d->flags            = 0;

	/* 
	 * Jiguo:
	 * This can be two different cases:
	 * 1. A local cached one is returned with a cbid
	 * 2. A cbuf item is obtained from the global free list without cbid
	 */
	if (d->cbid == 0) {
		/* Only created the first time we allocate a new cbuf */
		INIT_LIST(&d->owner, next, prev);
		cbid = cos_map_add(&cb_ids, cbuf_item);
	} else {
		/* We have a local, cached cbuf, so lets use its ID */
		cbid = cbuf_item->desc.cbid;
		/* 
		 * FIXME: What if this cbid is not the same as the one
		 * we setup above?  In this case, we have set up a mc
		 * in the client component for one cbid, and now
		 * allocated a different cbid.  The assert below might
		 * trigger.
		 */
	}
	ret           = d->cbid = cbid;
	mc            = __spd_cbvect_lookup_range(sti, cbid);
	assert(mc); 		/* see FIXME above */
	d->owner.meta = mc;

	mc->nfo.c.ptr = d->owner.addr >> PAGE_ORDER;
	if (tmem) {
		mc->sz              = PAGE_SIZE;
		mc->owner_nfo.thdid = cos_get_thd_id();
		mc->nfo.c.flags    |= CBUFM_IN_USE | CBUFM_TOUCHED | CBUFM_OWNER | 
			              CBUFM_TMEM   | CBUFM_WRITABLE;
		d->flags           |= CBUF_DESC_TMEM;
	} else {
		mc->sz                = size;
		mc->owner_nfo.c.nsent = mc->owner_nfo.c.nrecvd = 0;
		mc->nfo.c.flags      |= CBUFM_IN_USE | CBUFM_OWNER | CBUFM_WRITABLE;
	}
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
	struct spd_tmem_info *map_sti;
	DOUT("__cbuf_c_delete....cbid %d\n", cbid);

	__spd_cbvect_clean_val(sti, cbid);
	mman_revoke_page(cos_spd_id(), (vaddr_t)d->addr, 0);  // remove all mapped children

	m = FIRST_LIST(&d->owner, next, prev);
	
	while (m != &d->owner) {
		struct cb_mapping *n;

		/* remove from the vector in all mapped spds as well! */
		map_sti = get_spd_info(m->spd);
		DOUT("clear cbid %d in %d\n",cbid, map_sti->spdid);
		__spd_cbvect_clean_val(map_sti, cbid);

		valloc_free(cos_spd_id(), m->spd, (void *)(m->addr), 1);
		n = FIRST_LIST(m, next, prev);
		REM_LIST(m, next, prev);
		free(m);
		m = n;
	}

	valloc_free(cos_spd_id(), sti->spdid, (void *)(d->owner.addr), 1);
	sti->ci.spd_cinfo_page->cos_tmem_available[COMP_INFO_TMEM_CBUF]--;

	DOUT("unmapped is done\n");
	return 0;
}

/* 
 * FIXME: 1) reference counting so that components can maintain the
 * buffer if they please, 2) asynchronous (shmmem) notification of cb
 * deallocation.
 */
int
cbuf_c_delete(spdid_t spdid, int cbid)
{
	struct spd_tmem_info *sti;
	int ret = 0;  /* return value not used */

	TAKE();

	sti = get_spd_info(spdid);
	assert(sti);

	return_tmem(sti);

	RELEASE();
	return ret;
}

int
cbuf_c_retrieve(spdid_t spdid, int cbid, int len)
{
	int ret = -1;
	char *l_addr;
	vaddr_t d_addr;
	struct cos_cbuf_item *cbi;
	struct cb_desc *d;
	struct cb_mapping *m;
	struct spd_tmem_info *sti;
	struct cbuf_meta *mc;

	TAKE();

	cbi = cos_map_lookup(&cb_ids, cbid);
	if (!cbi) goto done;
	d = &cbi->desc;
	/* sanity and access checks */
	if (d->sz < len) goto done;
	m = malloc(sizeof(struct cb_mapping));
	if (!m) goto done;
	INIT_LIST(m, next, prev);

	d_addr = (vaddr_t)valloc_alloc(cos_spd_id(), spdid, 1);
	if (!d_addr) goto free;
	l_addr = d->addr;  //cbuf_item addr, initialized in cos_init()

	sti = get_spd_info(spdid);
	/* Make sure we have access to the component shared page */
	assert(SPD_IS_MANAGED(sti));
	mc = __spd_cbvect_lookup_range(sti, cbid);
	if (!mc) goto err;

	assert(d_addr && l_addr);
	if (unlikely(!mman_alias_page(cos_spd_id(), (vaddr_t)l_addr, spdid, d_addr))) {
		goto err;
	}

	m->cbd  = d;
	m->spd  = spdid;
	m->addr = d_addr;
	m->meta = mc;
	if (d->flags & CBUF_DESC_TMEM) {
		/* owners should not be cbuf2bufing their buffers. */
		assert(!(mc->nfo.c.flags & CBUFM_OWNER));
		mc->owner_nfo.thdid   = 0;
		mc->nfo.c.flags      |= CBUFM_IN_USE | CBUFM_TOUCHED | 
			                CBUFM_TMEM   | CBUFM_WRITABLE;
	} else {
		mc->owner_nfo.c.nsent = mc->owner_nfo.c.nrecvd = 0;
		mc->nfo.c.flags      |= CBUFM_IN_USE | CBUFM_WRITABLE;
	}
	mc->nfo.c.ptr = d_addr >> PAGE_ORDER;
	mc->sz        = d->sz;

	ADD_LIST(&d->owner, m, next, prev);
	ret = 0;
done:
	RELEASE();
	return ret;
err:
	valloc_free(cos_spd_id(), spdid, (void*)d_addr, 1);
free:
	free(m);
	goto done;
}

int
cbuf_c_introspect(spdid_t spdid, int iter)
{
	struct spd_tmem_info *sti;
	spdid_t s_spdid;
	struct cos_cbuf_item *cci = NULL, *list;
	
	int counter = 0;

	TAKE();

	sti = get_spd_info(spdid);
	assert(sti);
	s_spdid = sti->spdid;
	list = &spd_tmem_info_list[s_spdid].tmem_list;
	printc("try to find cbuf for this spd 1\n");
	if (iter == -1){
		for (cci = FIRST_LIST(list, next, prev) ;
		     cci != list;
		     cci = FIRST_LIST(cci, next, prev)) {
			printc("try to find cbuf for this spd 2\n");
			struct cbuf_meta cm;
			cm.nfo.v = cci->desc.owner.meta->nfo.v;
			if (CBUF_OWNER(cm.nfo.c.flags) && 
			    CBUF_IN_USE(cm.nfo.c.flags)) counter++;
		}
		RELEASE();
		return counter;
	} else {
		for (cci = FIRST_LIST(list, next, prev) ;
		     cci != list;
		     cci = FIRST_LIST(cci, next, prev)) {
			struct cbuf_meta cm;
			cm.nfo.v = cci->desc.owner.meta->nfo.v;
			if (CBUF_OWNER(cm.nfo.c.flags) && 
                            CBUF_IN_USE(cm.nfo.c.flags) &&
                            !(--iter)) goto found;
		}
	}
	
found:
	RELEASE();
	return cci->desc.cbid;
}

/* 
 * Exchange the cbuf descriptor (flags of ownership) of old spd and
 * requested spd
 */
static int 
mgr_update_owner(spdid_t new_spdid, long cbid)
{
	struct spd_tmem_info *old_sti, *new_sti;
	struct cb_desc *d;
	struct cos_cbuf_item *cbi;
	struct cb_mapping *old_owner, *new_owner, tmp;
	struct cbuf_meta *old_mc, *new_mc;
	vaddr_t mgr_addr;
	int ret = 0;

	cbi = cos_map_lookup(&cb_ids, cbid);
	if (!cbi) goto err;
	d = &cbi->desc;
	old_owner = &d->owner;
	old_sti = get_spd_info(old_owner->spd);
	assert(SPD_IS_MANAGED(old_sti));

	old_mc = __spd_cbvect_lookup_range(old_sti, cbid);
	if (!old_mc) goto err;
	if (!CBUF_OWNER(old_mc->nfo.c.flags)) goto err;
	for (new_owner = FIRST_LIST(old_owner, next, prev) ; 
	     new_owner != old_owner; 
	     new_owner = FIRST_LIST(new_owner, next, prev)) {
		if (new_owner->spd == new_spdid) break;
	}

	if (new_owner == old_owner) goto err;
	new_sti = get_spd_info(new_owner->spd);
	assert(SPD_IS_MANAGED(new_sti));

        // this returns the whole page for the range
	mgr_addr = __spd_cbvect_retrieve_page(old_sti, cbid); 
	assert(mgr_addr);
	__spd_cbvect_add_range(new_sti, cbid, mgr_addr);

	new_mc = __spd_cbvect_lookup_range(new_sti, cbid);
	if(!new_mc) goto err;
	new_mc->nfo.c.flags |= CBUFM_OWNER;
	old_mc->nfo.c.flags &= ~CBUFM_OWNER;	

	// exchange the spd and addr in cbuf_mapping
	tmp.spd = old_owner->spd;
	old_owner->spd = new_owner->spd;
	new_owner->spd = tmp.spd;

	tmp.addr = old_owner->addr;
	old_owner->addr = new_owner->addr;
	new_owner->addr = tmp.addr;
done:
	return ret;
err:
	ret = -1;
	goto done;
}


/* 
 * This is called when the component checks if it still owns the cbuf
 * or wants to hold a cbuf, if it is not the creater, the ownership
 * should be re-granted to it from the original owner. For example,
 * when the ramfs server is called and the server wants to keep the
 * cbuf longer before restore.(need remember which cbufs for that
 * tid??)
 *
 * r_spdid is the requested spd
 */
int   
cbuf_c_claim(spdid_t r_spdid, int cbid)
{
	int ret = 0;
	spdid_t o_spdid;
	struct cb_desc *d;
	struct cos_cbuf_item *cbi;

	assert(cbid >= 0);

	TAKE();
	cbi = cos_map_lookup(&cb_ids, cbid);
	if (!cbi) { 
		ret = -1; 
		goto done;
	}
	d = &cbi->desc;
	
	o_spdid = d->owner.spd;
	if (o_spdid == r_spdid) goto done;

	ret = mgr_update_owner(r_spdid, cbid); // -1 fail, 0 success
done:
	RELEASE();
	return ret;   
}

void 
cos_init(void *d)
{
	DOUT("CBUFMgr: %d in spd %ld cbuf mgr running.....\n", cos_get_thd_id(), cos_spd_id());
	LOCK_INIT();
	cos_map_init_static(&cb_ids);
	BUG_ON(cos_map_add(&cb_ids, NULL)); /* reserve id 0 */
	int i;

	memset(spd_tmem_info_list, 0, sizeof(struct spd_tmem_info) * MAX_NUM_SPDS);
    
	for(i = 0; i < MAX_NUM_SPDS; i++){
		spd_tmem_info_list[i].spdid = i;    
		INIT_LIST(&spd_tmem_info_list[i].ci, next, prev);
		INIT_LIST(&spd_tmem_info_list[i].tmem_list, next, prev);
		INIT_LIST(&spd_tmem_info_list[i].bthd_list, next, prev);
	}

	free_tmem_list = NULL;
	INIT_LIST(&global_blk_list, next, prev);

	tmems_allocated = 0;

	// Map all of the spds we can into this component
	for (i = 0 ; i < MAX_NUM_SPDS ; i++) {
		spdid_t spdid = i;

		void *hp;
		hp = valloc_alloc(cos_spd_id(), cos_spd_id(), 1);
		spdid = cinfo_get_spdid(i);
		if (!spdid) break;

		if(cinfo_map(cos_spd_id(), (vaddr_t)hp, spdid)){
			DOUT("Could not map cinfo page for %d\n", spdid);
			BUG();
		}
		spd_tmem_info_list[spdid].ci.spd_cinfo_page = hp;

		spd_tmem_info_list[spdid].ci.meta = NULL; 
		spd_tmem_info_list[spdid].managed = 1;

		spd_tmem_info_list[spdid].relinquish_mark = 0; 
    
		tmems_target += DEFAULT_TARGET_ALLOC;
		spd_tmem_info_list[spdid].num_allocated = 0;
		spd_tmem_info_list[spdid].num_desired = DEFAULT_TARGET_ALLOC;
		spd_tmem_info_list[spdid].num_blocked_thds = 0;
		spd_tmem_info_list[spdid].num_waiting_thds = 0;
		spd_tmem_info_list[spdid].num_glb_blocked = 0;
		spd_tmem_info_list[spdid].ss_counter = 0;
		spd_tmem_info_list[spdid].ss_max = MAX_NUM_MEM;
		empty_comps++;
	}
	over_quota_total = 0;
	over_quota_limit = MAX_NUM_MEM;

	event_waiting();
	return;
}
 
void 
cbufmgr_buf_report(void)
{
	tmem_report();
	/* int i; */
	/* for (i = 0 ; i < MAX_NUM_SPDS ; i++) { */
	/* 	spdid_t spdid = i; */

	/* 	if (!spd_tmem_info_list[spdid].managed) continue; */

	/* 	printc("spdid %d: allocated %d, desired %d, blked %d, glb_blked %d, ss %d\n", */
	/* 	       spd_tmem_info_list[spdid].spdid, */
	/* 	       spd_tmem_info_list[spdid].num_allocated, */
	/* 	       spd_tmem_info_list[spdid].num_desired, */
	/* 	       spd_tmem_info_list[spdid].num_blocked_thds, */
	/* 	       spd_tmem_info_list[spdid].num_glb_blocked, */
	/* 	       spd_tmem_info_list[spdid].ss_counter); */
	/* } */
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

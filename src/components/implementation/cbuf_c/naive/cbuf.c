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
#include <mem_mgr.h>

//#define PRINCIPAL_CHECKS

struct cb_desc;
struct cb_mapping {
	spdid_t spd;
	vaddr_t addr;		/* other component's map address */
	struct cb_mapping *next, *prev;
	struct cb_desc *cbd;
};

/* Data we wish to track for every cbuf */
struct cb_desc {
	u16_t principal;	/* principal that owns the memory */
	int cbid;		/* cbuf id */
	int obj_sz;
	void *addr; 	/* local map address */
	struct cb_mapping owner;
};

COS_MAP_CREATE_STATIC(cb_ids);
cos_lock_t l;
#define TAKE() lock_take(&l);
#define RELEASE() lock_release(&l);

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
	h = cos_get_heap_ptr();
	cos_set_heap_ptr(h + PAGE_SIZE);
	if (!mman_get_page(cos_spd_id(), (vaddr_t)h, 0)) goto err;
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
	cos_set_heap_ptr(h - PAGE_SIZE);
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
	lock_init(&l);
	cos_map_init_static(&cb_ids);
	BUG_ON(cos_map_add(&cb_ids, NULL)); /* reserve id 0 */
}

void
bin(void)
{
	sched_block(cos_spd_id(), 0);
}


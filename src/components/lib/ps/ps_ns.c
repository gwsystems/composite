/***
 * Copyright 2015 by Gabriel Parmer.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Authors: Gabriel Parmer, gparmer@gwu.edu, 2015
 */

#include <ps_ns.h>

/* The slab allocator for slab heads that are not internal to the slab itself */
PS_SLAB_CREATE_DEF(slabhead, sizeof(struct ps_slab))

/* 
 * Namespace allocators make sure that the slab head is allocated
 * separately from the memory itself so that all lookups within the
 * lookup tree are properly aligned.
 *
 * FIXME: this is a scalability bottleneck.  A single list for
 * balancing all namespaces that are freed (i.e. when a slab is
 * deallocated).  This makes balancing faster, which is a significant
 * benefit.
 */
struct ps_slab *
ps_slab_nsalloc(struct ps_mem *m, size_t sz, coreid_t coreid)
{ 
	ps_desc_t id = 0, range = m->ns_info.desc_range;
	struct ps_slab *s;
	int newslab = 0;
	void *mem;
	struct ps_ns_info *nsi;
	(void)coreid; 

	ps_lock_take(&m->ns_info.lock);
	s = m->ns_info.fl.list;
	if (s) __slab_freelist_rem(&m->ns_info.fl, s);
	ps_lock_release(&m->ns_info.lock);

	if (!s) {
		id = ps_faa(&m->ns_info.frontier, range);
		if (unlikely(id >= m->ns_info.desc_max)) goto reset_frontier;
		s = ps_slab_alloc_slabhead();
		if (unlikely(!s))                        goto reset_frontier;

		s->start = id;
		s->end   = s->start + range;
		newslab  = 1;
	}

 	assert(!s->memory);
	mem = ps_plat_alloc(sz, coreid);
	if (unlikely(!mem)) goto free_slab;
	memset(mem, 0, sz);
	s->memory = mem;

	/* Add the slab's identities to the lookup table */
	nsi = &m->ns_info;
	assert(!nsi->lkupfn(nsi->ert, s->start, nsi->ert_depth, NULL));
	if (nsi->expandfn(nsi->ert,   s->start, nsi->ert_depth, NULL, mem, NULL) != 0) goto free_mem;
	assert(nsi->lkupfn(nsi->ert,  s->start, nsi->ert_depth, NULL) == mem);

	return s;
free_mem:
	ps_plat_free(mem, sz, coreid);
free_slab:
	ps_slab_free_slabhead(s);
reset_frontier:
	/* possible to leak namespace if many threads race between faa and here */
	if (newslab) ps_cas(&m->ns_info.frontier, id+range, id); 

	return NULL;
}

void
ps_slab_nsfree(struct ps_mem *m, struct ps_slab *s, size_t sz, coreid_t coreid)
{ 
	struct ps_ns_info *nsi;
	struct ert_intern *intern;

	ps_plat_free(s->memory, sz, coreid);

	/* Remove the reference in the lookup table to the slab */
	nsi = &m->ns_info;
	if (nsi->ert_depth > 1) {
		intern = nsi->lkupfn(nsi->ert, s->start, nsi->ert_depth-1, NULL);
		assert(intern->next == s->memory);
		intern->next = NULL;
		assert(!nsi->lkupfn(nsi->ert, s->start, nsi->ert_depth, NULL));
	}
	s->memory = NULL;

	ps_lock_take(&m->ns_info.lock);
	__slab_freelist_add(&m->ns_info.fl, s);
	ps_lock_release(&m->ns_info.lock);
}

void
ps_ns_init(struct ps_mem *m, void *ert, ps_lkupan_fn_t lkup, ps_expand_fn_t expand, size_t depth, ps_desc_t maxid, size_t range)
{
	struct ps_ns_info *ni;
	static unsigned long executed = 0;

	if (executed == 0 && ps_faa(&executed, 1) == 0) ps_slab_init_slabhead();

	ni = &m->ns_info;
	ni->desc_max   = maxid;
	ni->desc_range = range;
	ni->fl.list    = NULL;
	ni->frontier   = 0;
	ni->ert        = ert;
	ni->ert_depth  = depth;
	ni->lkupfn     = lkup;
	ni->expandfn   = expand;
	ps_lock_init(&ni->lock);
}

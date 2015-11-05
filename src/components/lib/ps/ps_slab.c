/***
 * Copyright 2011-2015 by Gabriel Parmer.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gparmer@gwu.edu, 2011
 *
 * History:
 * - Initial slab allocator, 2011
 * - Adapted for parsec, 2015
 */

#include <ps_slab.h>

/* 
 * Default allocation and deallocation functions: assume header is
 * internal to the slab's memory
 */
struct ps_slab *
ps_slab_defalloc(size_t sz, coreid_t coreid)
{ 
	struct ps_slab *s = ps_plat_alloc(sz, coreid);
	(void)coreid; 

	s->memory = s;
	return s;
}

void
ps_slab_deffree(struct ps_slab *s, size_t sz, coreid_t coreid)
{ ps_plat_free(s, sz, coreid); }

void
__ps_slab_init(struct ps_slab *s, struct ps_slab_info *si, PS_SLAB_PARAMS)
{
	size_t nfree, i;
	size_t objmemsz  = __ps_slab_objmemsz(obj_sz);
	struct ps_mheader *alloc, *prev;
	PS_SLAB_DEWARN;

	s->nfree  = nfree = (allocsz - headoff) / objmemsz;
	s->memsz  = allocsz;
	s->coreid = ps_coreid();

	/*
	 * Set up the slab's freelist
	 *
	 * TODO: cache coloring
	 */
	alloc = (struct ps_mheader *)((char *)s->memory + headoff);
	prev  = s->freelist = alloc;
	for (i = 0 ; i < nfree ; i++, prev = alloc, alloc = (struct ps_mheader *)((char *)alloc + objmemsz)) {
		__ps_mhead_init(alloc, s);
		prev->next = alloc;
	}
	__ps_slab_check_consistency(s);
	/* better not overrun memory */
	assert((void *)alloc <= (void *)((char*)s->memory + allocsz));

	ps_list_init(s, list);
	__slab_freelist_add(&si->fl, s);
}

void
__ps_slab_mem_remote_free(struct ps_mem *mem, struct ps_mheader *h, coreid_t core_target)
{
	struct ps_slab_remote_list *r = &mem->percore[core_target].slab_remote;

	ps_lock_take(&r->lock);
	__ps_qsc_enqueue(&r->remote_frees, h);
	ps_lock_release(&r->lock);
}

void
__ps_slab_mem_remote_process(struct ps_mem *mem, PS_SLAB_PARAMS)
{
	struct ps_slab_remote_list *r = &(mem->percore[coreid].slab_remote);
	struct ps_mheader      *h, *n;
	PS_SLAB_DEWARN;

	ps_lock_take(&r->lock);
	h = __ps_qsc_clear(&r->remote_frees);
	ps_lock_release(&r->lock);

	while (h) {
		__ps_slab_mem_free(__ps_mhead_mem(h), mem, PS_SLAB_ARGS);
		n       = h->next;
		h->next = NULL;
		h       = n;
	}
}

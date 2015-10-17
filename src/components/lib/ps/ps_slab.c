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

void
__ps_slab_init(struct ps_slab *s, struct ps_slab_info *si, size_t obj_sz, int allocsz, int hintern)
{
	size_t nfree, i;
	size_t start_off = sizeof(struct ps_slab) * hintern; /* hintern \in {0, 1}*/
	size_t objmemsz  = __ps_slab_objmemsz(obj_sz);
	struct ps_mheader *alloc, *prev;
	void *u = s; 		/* untyped slab, for memory arithmetic */

	assert(hintern == 0 || hintern == 1);
	/* division should be statically calculated with enough inlining */
	s->nfree  = nfree = (allocsz - start_off) / objmemsz;
	s->memsz  = allocsz;
	s->memory = s;
	s->coreid = ps_coreid();

	/*
	 * Set up the slab's freelist
	 *
	 * TODO: cache coloring
	 */
	alloc = (struct ps_mheader *)((char *)u + start_off);
	prev  = s->freelist = alloc;
	for (i = 0 ; i < nfree ; i++, prev = alloc, alloc = (struct ps_mheader *)((char *)alloc + objmemsz)) {
		__ps_mhead_init(alloc, s);
		prev->next = alloc;
	}
	/* better not overrun memory */
	assert((void *)alloc <= (void *)((char*)s + allocsz));

	ps_list_init(s, list);
	__slab_freelist_add(&si->fl, s);
}

void
__ps_slab_mem_remote_free(struct ps_mem_percore *fls, struct ps_mheader *h, u16_t core_target)
{
	struct ps_slab_remote_list *r = &fls[core_target].slab_remote;

	ps_lock_take(&r->lock);
	__ps_qsc_enqueue(&r->remote_frees, h);
	ps_lock_release(&r->lock);
}

void
__ps_slab_mem_remote_process(struct ps_mem_percore *percpu, size_t obj_sz, size_t allocsz, int hintern)
{
	struct ps_mem_percore      *m = &percpu[ps_coreid()];
	struct ps_slab_remote_list *r = &m->slab_remote;
	struct ps_mheader      *h, *n;

	ps_lock_take(&r->lock);
	h = __ps_qsc_clear(&r->remote_frees);
	ps_lock_release(&r->lock);

	while (h) {
		__ps_slab_mem_free(__ps_mhead_mem(h), percpu, obj_sz, allocsz, hintern);
		n       = h->next;
		h->next = NULL;
		h       = n;
	}
}

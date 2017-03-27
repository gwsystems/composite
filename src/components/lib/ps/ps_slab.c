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
ps_slab_defalloc(struct ps_mem *m, size_t sz, coreid_t coreid)
{ 
	struct ps_slab *s = ps_plat_alloc(sz, coreid);
	(void)coreid; (void)m;

	if (!s) return NULL;
	s->memory = s;
	return s;
}

void
ps_slab_deffree(struct ps_mem *m, struct ps_slab *s, size_t sz, coreid_t coreid)
{ (void)m; ps_plat_free(s, sz, coreid); }

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
	/* better not overrun memory */
	assert((void *)alloc <= (void *)((char*)s->memory + allocsz));

	ps_list_init(s, list);
	__slab_freelist_add(&si->fl, s);
	__ps_slab_freelist_check(&si->fl);
}

void
ps_slabptr_init(struct ps_mem *m)
{
	int i;

	memset(m, 0, sizeof(struct ps_mem));

	for (i = 0 ; i < PS_NUMCORES ; i++) {
		struct ps_slab_remote_list *l;
		struct ps_slab_info *si = &m->percore[i].slab_info;
		int j;

		si->fl.list      = NULL;
		si->salloccnt    = 0;
		si->remote_token = 0;
		for (j = 0 ; j < PS_NUMLOCALITIES ; j++) {
			l = &m->percore[i].slab_remote[j];
			ps_lock_init(&l->lock);
			__ps_qsc_clear(&l->remote_frees);
		}
	}
}

void
ps_slabptr_stats(struct ps_mem *m, struct ps_slab_stats *stats)
{
	int i, j;
	struct ps_slab *s;
	struct ps_mem_percore *pc;

	memset(stats, 0, sizeof(struct ps_slab_stats));
	
	for (i = 0 ; i < PS_NUMCORES ; i++) {
		pc = &m->percore[i];
		s = pc->slab_info.fl.list;
		stats->percore[i].nslabs = pc->slab_info.nslabs;
		do {
			if (!s) break;
			stats->percore[i].npartslabs++;
			stats->percore[i].nfree += s->nfree;
			s = ps_list_next(s, list);
		} while (s != pc->slab_info.fl.list);

		for (j = 0 ; j < PS_NUMLOCALITIES ; j++) {
			stats->percore[i].nremote += pc->slab_remote[j].nfree;
		}
	}
}

int
ps_slabptr_isempty(struct ps_mem *m)
{
	int i, j;

	for (i = 0 ; i < PS_NUMCORES ; i++) {
		if (m->percore[i].slab_info.nslabs) return 0;
		for (j = 0 ; j < PS_NUMLOCALITIES ; j++) {
			if (m->percore[i].slab_remote[j].nfree) return 0;
		}
	}
	return 1;
}


void
__ps_slab_mem_remote_free(struct ps_mem *mem, struct ps_mheader *h, coreid_t core_target)
{
	struct ps_slab_remote_list *r;
	coreid_t     tmpcoreid;
	localityid_t numaid;
	
	ps_tsc_locality(&tmpcoreid, &numaid);
	r = &mem->percore[core_target].slab_remote[numaid];

	ps_lock_take(&r->lock);
	__ps_qsc_enqueue(&r->remote_frees, h);
	r->nfree++;
	ps_lock_release(&r->lock);
}

/* 
 * This function wants to contend cache-lines with another numa chip
 * at most once, or else the latency will blow up.  It can detect this
 * contention fairly well with the fact that there are, or aren't,
 * aren't any items in the remote freelist.  Thus, this function
 * processes the remote free lists for exactly _one_ remote numa node
 * each time it is called.
 */
void
__ps_slab_mem_remote_process(struct ps_mem *mem, struct ps_slab_info *si, PS_SLAB_PARAMS)
{
	struct ps_mheader *h, *n;
	unsigned long locality = si->remote_token;
	PS_SLAB_DEWARN;

	do {
		struct ps_slab_remote_list *r = &mem->percore[coreid].slab_remote[locality];

		ps_lock_take(&r->lock);
		h        = __ps_qsc_clear(&r->remote_frees);
		r->nfree = 0;
		ps_lock_release(&r->lock);
		locality = (locality + 1) % PS_NUMLOCALITIES;
	} while (!h && locality != si->remote_token);

	si->remote_token = locality;

	while (h) {
		n       = h->next;
		h->next = NULL;
		__ps_slab_mem_free(__ps_mhead_mem(h), mem, PS_SLAB_ARGS);
		h       = n;
	}
}

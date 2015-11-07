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
	ps_desc_t id, range = m->ns_info.desc_range;
	struct ps_slab *s;
	void *mem;
	(void)coreid; 

	ps_lock_take(&m->ns_info.lock);
	s = m->ns_info.fl.list;
	if (s) __slab_freelist_rem(&m->ns_info.fl, s);
	ps_lock_release(&m->ns_info.lock);

	if (!s) {
		id = ps_faa(&m->ns_info.frontier, range);
		if (unlikely(id >= m->ns_info.desc_max)) goto reset_frontier;
		s = ps_slab_alloc_slabhead();
		if (unlikely(!s)) goto reset_frontier;

		s->start = id;
		s->end   = s->start + range;
	}
	assert(!s->memory);

	printf("nsalloc(sz %lu), start %lu, range %lu\n", sz, s->start, range);

	mem = ps_plat_alloc(sz, coreid);
	memset(mem, 0, sz);
	if (unlikely(!mem)) goto free_slab;
	s->memory = mem;

	return s;
free_slab:
	ps_slab_free_slabhead(s);
reset_frontier:
	ps_faa(&m->ns_info.frontier, -range);
	
	return NULL;
}

void
ps_slab_nsfree(struct ps_mem *m, struct ps_slab *s, size_t sz, coreid_t coreid)
{ 
	ps_plat_free(s->memory, sz, coreid); 
	s->memory = NULL;

	ps_lock_take(&m->ns_info.lock);
	__slab_freelist_add(&m->ns_info.fl, s);
	ps_lock_release(&m->ns_info.lock);

	printf("nsfree(sz %lu), start %lu\n", sz, s->start);
}

void
ps_ns_init(struct ps_mem *m, ps_desc_t maxid, size_t range)
{
	struct ps_ns_info *ni;
	
	ni = &m->ns_info;
	ni->desc_max   = maxid;
	ni->desc_range = range;
	ni->fl.list    = NULL;
	ni->frontier   = 0;
	ps_lock_init(&ni->lock);
}

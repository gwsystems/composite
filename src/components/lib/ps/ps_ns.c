#include <ps_ns.h>

/* The slab allocator for slab heads that are not internal to the slab itself */
PS_SLAB_CREATE_DEF(slabhead, sizeof(struct ps_slab))

/* 
 * Namespace allocators make sure that the slab head is allocated
 * separately from the memory itself so that all lookups within the
 * lookup tree are properly aligned.
 */
struct ps_slab *
ps_slab_nsalloc(size_t sz, coreid_t coreid)
{ 
	struct ps_slab *s = ps_slab_alloc_slabhead();
	void *m;
	(void)coreid; 

	if (unlikely(!s)) return NULL;
	m = ps_plat_alloc(sz, coreid);
	if (unlikely(!m)) {
		ps_slab_free_slabhead(s);
		return NULL;
	}
	s->memory = m;

	return s;
}

void
ps_slab_nsfree(struct ps_slab *s, size_t sz, coreid_t coreid)
{ 
	ps_plat_free(s->memory, sz, coreid); 
	ps_slab_free_slabhead(s);
}

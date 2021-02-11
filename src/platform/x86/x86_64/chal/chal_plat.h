#ifndef CHAL_PLAT_H
#define CHAL_PLAT_H

/* This flushes all levels of cache of the current logical CPU. */
static inline void
chal_flush_cache(void)
{
	asm volatile("wbinvd" : : : "memory");
}

static inline void
chal_flush_tlb_global(void)
{
}
static inline void
chal_remote_tlb_flush(int target_cpu)
{
}
/* This won't flush global TLB (pinned with PGE) entries. */
static inline void
chal_flush_tlb(void)
{
}

static inline void *
chal_pa2va(paddr_t address)
{
	return (void *)(address + COS_MEM_KERN_START_VA);
}

static inline paddr_t
chal_va2pa(void *address)
{
	return (paddr_t)(address - COS_MEM_KERN_START_VA);
}

#endif /* CHAL_PLAT_H */

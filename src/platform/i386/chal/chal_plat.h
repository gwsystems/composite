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

#endif /* CHAL_PLAT_H */

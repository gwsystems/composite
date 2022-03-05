#ifndef CHAL_PLAT_H
#define CHAL_PLAT_H

int chal_tlb_lockdown(unsigned long entryid, unsigned long vaddr, unsigned long paddr);
int chal_l1flush(void);
int chal_tlbstall(void);
int chal_tlbstall_recount(int a);
int chal_tlbflush(int a);


#if defined(__x86_64__)
#define CR3_NO_FLUSH (1ul << 63)
#endif

/* Reference Intel 64 and IA-32 Architecture Software Developer's Manual, Volume 2 */
#define INVPCID_TYPE_INDIVIDUAL_ADDR    0 /* invalidate all tlb entries for a PCID used to map a vaddr */
#define INVPCID_TYPE_SINGLE_CONTEXT     1 /* invalidate all tlb entries (not global) for a PCID */
#define INVPCID_TYPE_ALL_CONTEXT_GLOBAL 2 /* invalidate all tlb entries, including global */
#define INVPCID_TYPE_ALL_CONTEXT        3 /* invalidate all tlb entries, not including global */

static inline void
__invpcid(u64_t pcid, u64_t addr, unsigned long type)
{
	/* 
	 * invpcid takes a 128 bit value from memory as: | address | 000...0 | pcid | 
	 *                                              127        63        11     0
	 * This is the case in both 32 and 64 bit execution modes
	 */ 
	struct { u64_t pcid; u64_t addr; } desc = { .pcid = pcid, .addr = addr };

	asm volatile("invpcid %0, %1" : : "m"(desc), "r"(type) : "memory");
}

#endif

static inline unsigned long
__readcr3(void)
{
	unsigned long val;
	asm volatile("mov %%cr3, %0" : "=r"(val));
	return val;
}

static inline void
__writecr3(unsigned long val)
{
	asm volatile("mov %0, %%cr3" : : "r"(val));
}

/* This flushes all levels of cache of the current logical CPU. */
static inline void
chal_flush_cache(void)
{
	asm volatile("wbinvd" : : : "memory");
}

/* This won't flush global TLB (pinned with PGE) entries. */ 
static inline void
chal_flush_tlb(void)
{
	/* FIXME: what if no invpcid (pre Intel Haswell and AMD Zen 3) */
	/* faster than cr3 r+w */
	__invpcid(0, 0, INVPCID_TYPE_ALL_CONTEXT);
}

static inline void
chal_flush_tlb_global(void)
{
	/* FIXME: what if no invpcid (pre Intel Haswell and AMD Zen 3) */
	/* faster than cr4 r+w */
	__invpcid(0, 0, INVPCID_TYPE_ALL_CONTEXT_GLOBAL);
}

static inline void
chal_flush_tlb_asid(asid_t asid)
{
	__invpcid((u64_t)asid, 0, INVPCID_TYPE_SINGLE_CONTEXT);
}

static inline void
chal_remote_tlb_flush(int target_cpu)
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
	return (paddr_t)((unsigned long)address - COS_MEM_KERN_START_VA);
}

#endif /* CHAL_PLAT_H */

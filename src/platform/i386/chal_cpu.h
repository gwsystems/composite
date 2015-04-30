#ifndef CHAL_CPU_H
#define CHAL_CPU_H

#include <pgtbl.h>
#include "isr.h"

typedef enum {
	CR4_TSD    = 1<<1, 	/* time stamp (rdtsc) access at user-level disabled */
	CR4_PSE    = 1<<3, 	/* page size extensions (superpages) */
	CR4_PGE    = 1<<6, 	/* page global bit enabled */
	CR4_PCE    = 1<<7, 	/* user-level access to performance counters enabled (rdpmc) */
	CR4_OSFXSR = 1<<8, 	/* floating point enabled */
	CR4_SMEP   = 1<<19, 	/* Supervisor Mode Execution Protection Enable */
	CR4_SMAP   = 1<<20	/* Supervisor Mode Access Protection Enable */
} cr4_flags_t;

static inline u32_t
chal_cpu_cr4_get(void)
{
	u32_t config;
	asm("movl %%cr4, %0" : "=r"(config));
	return config;
}

static inline void
chal_cpu_cr4_set(cr4_flags_t flags)
{
	u32_t config = chal_cpu_cr4_get();
	config |= (u32_t)flags;
	asm("movl %0, %%cr4" : : "r"(config));
}

static void
chal_cpu_init(void)
{
	chal_cpu_cr4_set(CR4_PSE | CR4_PGE | CR4_PCE);
}

static inline vaddr_t
chal_cpu_fault_vaddr(struct registers *r)
{
	vaddr_t fault_addr;
	asm volatile("mov %%cr2, %0" : "=r" (fault_addr));	
	return fault_addr;
}

/* FIXME: I doubt these flags are really the same as the PGTBL_* macros */
static inline u32_t
chal_cpu_fault_errcode(struct registers *r) { return r->err_code; }

#endif /* CHAL_CPU_H */

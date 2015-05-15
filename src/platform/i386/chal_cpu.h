#ifndef CHAL_CPU_H
#define CHAL_CPU_H

#include <pgtbl.h>
#include "isr.h"

typedef enum {
	CR4_TSD    = 1<<2, 	/* time stamp (rdtsc) access at user-level disabled */
	CR4_PSE    = 1<<4, 	/* page size extensions (superpages) */
	CR4_PGE    = 1<<7, 	/* page global bit enabled */
	CR4_PCE    = 1<<8, 	/* user-level access to performance counters enabled (rdpmc) */
	CR4_OSFXSR = 1<<9, 	/* floating point enabled */
	CR4_SMEP   = 1<<20, 	/* Supervisor Mode Execution Protection Enable */
	CR4_SMAP   = 1<<21	/* Supervisor Mode Access Protection Enable */
} cr4_flags_t;

enum {
	CR0_PG    = 1<<31, 	/* enable paging */
	CR0_FPEMU = 1<<2,	/* disable floating point, enable emulation */
	CR0_PRMOD = 1<<0	/* in protected-mode (vs real-mode) */
};

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
chal_cpu_paging_activate(pgtbl_t pgtbl)
{
//	unsigned long cr0;

	pgtbl_update(pgtbl);
	/* asm volatile("mov %%cr0, %0" : "=r"(cr0)); */
	/* cr0 |= CR0_PG; */
	/* printk("cr0 = %x\n", cr0); */
	/* asm volatile("mov %0, %%cr0" : : "r"(cr0)); */
}

static void
chal_cpu_init(void)
{
	u32_t cr4 = chal_cpu_cr4_get();
	chal_cpu_cr4_set(cr4 | CR4_PSE | CR4_PGE);
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

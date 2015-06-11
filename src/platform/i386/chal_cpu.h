#ifndef CHAL_CPU_H
#define CHAL_CPU_H

#include <pgtbl.h>
#include "isr.h"
#include "tss.h"

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

#define IA32_SYSENTER_CS  0x174
#define IA32_SYSENTER_ESP 0x175
#define IA32_SYSENTER_EIP 0x176

extern void sysenter_entry(void);

static inline void
writemsr(u32_t reg, u32_t low, u32_t high)
{
	__asm__("wrmsr" : : "c"(reg), "a"(low), "d"(high));
}

static void
chal_cpu_init(void)
{
	u32_t cr4 = chal_cpu_cr4_get();
	chal_cpu_cr4_set(cr4 | CR4_PSE | CR4_PGE);
	writemsr(IA32_SYSENTER_CS, SEL_KCSEG, 0);
	writemsr(IA32_SYSENTER_ESP, (u32_t)tss.esp0, 0);
	writemsr(IA32_SYSENTER_EIP, (u32_t)sysenter_entry, 0);	
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

static inline void
chal_user_upcall(void *ip)
{
	__asm__("sti ; sysexit" : : "c"(0), "d"(ip));
}

#endif /* CHAL_CPU_H */

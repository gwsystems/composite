#pragma once

#include <types.h>
#include <consts.h>
#include <chal_cpu.h>

#define PGTBL_ARCH_ENTRY_NULL 0	/* not present, and no referenced page */
#define PGTBL_ARCH_PERM_MASK  ((1 << COS_PAGE_ORDER) - 1)
#define PGTBL_ARCH_ENTRY_MASK (~PGTBL_ARCH_PERM_MASK)

struct pgtbl_top {
	pgtbl_t next[COS_PGTBL_TOP_NENT];
	pgtbl_t kern_next[COS_PGTBL_KERN_NENT];
};

struct pgtbl_internal {
	pgtbl_t next[COS_PGTBL_INTERNAL_NENT];
};

void         pgtbl_top_initialize(struct pgtbl_top *pt);
void         pgtbl_intern_initialize(struct pgtbl_internal *pt);
int          pgtbl_arch_entry_empty(pgtbl_t entry);

static inline pgtbl_t
pgtbl_arch_entry_pack(pageref_t ref, uword_t perm)
{
	return chal_va2pa(ref2page_ptr(ref)) | perm;
}

static inline void
pgtbl_arch_entry_unpack(pgtbl_t entry, pageref_t *ref, uword_t *perm)
{
	if (ref  != NULL) *ref  = page2ref(chal_pa2va(entry & PGTBL_ARCH_ENTRY_MASK));
	if (perm != NULL) *perm = entry & PGTBL_ARCH_PERM_MASK;
}

static inline void
pgtbl_arch_activate(pgtbl_t pgtbl, prot_domain_tag_t tag)
{
	/* TODO: propertly use tag as PCID and/or MPK key */
	chal_cpu_pgtbl_activate(pgtbl | (((u64_t)tag) & PGTBL_ARCH_PERM_MASK));
}

#pragma once

#include "chal_types.h"
#include "cos_types.h"
#include <types.h>
#include <chal_consts.h>

#define PGTBL_ARCH_ENTRY_NULL 0	/* not present, and no referenced page */

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
	return (ref << 12) & perm;
}

static inline void
pgtbl_arch_entry_unpack(pgtbl_t entry, pageref_t *ref, uword_t *perm)
{
	if (ref  != NULL) *ref  = entry >> 12;
	if (perm != NULL) *perm = entry & ((1 << 12) - 1);
}

static inline void
pgtbl_arch_activate(pgtbl_t pgtbl, prot_domain_tag_t tag)
{
	u64_t tag64 = tag;
	u64_t ts    = (tag64 & 0xFFFF) | ((tag64 >> 16) << 48);
	u64_t pt    = pgtbl | ts;

	asm volatile("movq %0, %%cr3" : : "r"(pt));
}

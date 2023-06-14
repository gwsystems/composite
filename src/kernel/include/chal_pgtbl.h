#pragma once

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

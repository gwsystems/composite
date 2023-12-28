#include <pgtbl.h>

int
pgtbl_arch_entry_empty(pgtbl_t entry)
{
	return entry == PGTBL_ARCH_ENTRY_NULL;
}

extern void kern_pgtbl_init(struct pgtbl_top *top);

void
pgtbl_top_initialize(struct pgtbl_top *pt)
{
	int i;

	/* The top-level of a page-table must include the kernel mappings. */
	for (i = 0; i < COS_PGTBL_TOP_NENT; i++) {
		pt->next[i] = PGTBL_ARCH_ENTRY_NULL;
	}
	kern_pgtbl_init(pt);
}

void
pgtbl_intern_initialize(struct pgtbl_internal *pt)
{
	/*
	 * we have an internal page-table. Zeroing it out
	 * should yield entries with vacuous permissions.
	 */
	page_zero((struct page *)pt);
}

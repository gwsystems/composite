#include <pgtbl.h>

int
pgtbl_arch_entry_empty(pgtbl_t entry)
{
	return entry == PGTBL_ARCH_ENTRY_NULL;
}

void
pgtbl_top_initialize(struct pgtbl_top *pt)
{
	int i;

	/* The top-level of a page-table must include the kernel mappings. */
	for (i = 0; i < COS_PGTBL_TOP_NENT; i++) {
		pt->next[i] = 0;
	}
	for (i = 0; i < COS_PGTBL_KERN_NENT; i++) {
		pt->kern_next[i] = 0; /* TODO: copy kern mappings */
	}
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

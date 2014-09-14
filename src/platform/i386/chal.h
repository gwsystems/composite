/**
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Copyright The George Washington University, Gabriel Parmer,
 * gparmer@gwu.edu, 2012
 */

#include "chal.h"

/* 
 * Switch to the specified page-tables.  This will not only switch the
 * loaded page tables on the current cpu, but also any backing
 * data-structures that are tracked in the platform code.
 * 
 * This function must be specified in the chal_plat.h file.
 */
static inline void chal_pgtbl_switch(paddr_t pt);

/* 
 * Switch any backing data-structures for the "current" page-table,
 * but _not_ the actual loaded page-tables.
 * 
 * This function must be specified in the chal_plat.h file.
 */
static inline void __chal_pgtbl_switch(paddr_t pt);

/* Add a page to pgtbl at address. 0 on success */
int     chal_pgtbl_add(paddr_t pgtbl, vaddr_t vaddr, paddr_t paddr);

/* Translate a vaddr to an addressable address via pgtbl */
vaddr_t chal_pgtbl_vaddr2kaddr(paddr_t pgtbl, unsigned long addr);

/* Remove mapping for a vaddr from pgtbl. != 0 if mapping doesn't exist */
paddr_t chal_pgtbl_rem(paddr_t pgtbl, vaddr_t va);
int     chal_pgtbl_entry_absent(paddr_t pt, unsigned long addr);
void    chal_pgtbl_copy_range(paddr_t pt_to, paddr_t pt_from,
			      unsigned long lower_addr, unsigned long size);
void    chal_pgtbl_copy_range_nocheck(paddr_t pt_to, paddr_t pt_from,
				      unsigned long lower_addr, unsigned long size);
void    chal_pgtbl_zero_range(paddr_t pt, unsigned long lower_addr, unsigned long size);

/* can we switch the current page tables right now? */
int     chal_pgtbl_can_switch(void);

/* operations on the page directory (as opposed to on page-table entries) */
int chal_pgtbl_add_middledir(paddr_t pt, unsigned long vaddr);
int chal_pgtbl_rem_middledir(paddr_t pt, unsigned long vaddr);
int chal_pgtbl_rem_middledir_range(paddr_t pt, unsigned long vaddr, long size);
int chal_pgtbl_add_middledir_range(paddr_t pt, unsigned long vaddr, long size);

/************************************
 * Page allocation and deallocation *
 ************************************/

void *chal_alloc_page(void);
void chal_free_page(void *page);

/*******************
 * Other functions *
 *******************/

int chal_attempt_brand(struct thread *brand);

void chal_idle(void);

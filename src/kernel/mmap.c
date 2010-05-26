/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include "include/mmap.h"

static struct cos_page cos_pages[COS_MAX_MEMORY];

extern void *cos_alloc_page(void);
extern void *cos_free_page(void *page);
extern void *va_to_pa(void *va);
extern void *pa_to_va(void *pa);

void cos_init_memory(void) 
{
	int i;

	for (i = 0 ; i < COS_MAX_MEMORY ; i++) {
		cos_pages[i].addr = 0;
	}

	return;
}

void cos_shutdown_memory(void)
{
	int i;

	for (i = 0 ; i < COS_MAX_MEMORY ; i++) {
		paddr_t addr = cos_pages[i].addr;

		if (0 != addr) {
			cos_free_page(pa_to_va((void*)addr));
			addr = 0;
		}
	}
}

/*
 * This would be O(1) in the real implementation as there is a 1-1
 * correspondence between phys pages and memory capabilities, but in
 * our Linux implementation, this is not so.  The least we could do is
 * keep the page sorted by physaddr and do a binary search here.
 */
int cos_paddr_to_cap(paddr_t pa)
{
	int i;

	for (i = 0 ; i < COS_MAX_MEMORY ; i++) {
		if (cos_pages[i].addr == pa) {
			return i;
		}
	}

	return 0;
}   

paddr_t cos_access_page(unsigned long cap_no)
{
	paddr_t addr;

	if (cap_no > COS_MAX_MEMORY) return 0;

	addr = cos_pages[cap_no].addr;
	if (0 == addr) {
		void *r = cos_alloc_page();

		if (NULL == r) {
			printk("cos: could not allocate page for cos memory\n");
			return 0;
		}
		addr = cos_pages[cap_no].addr = (paddr_t)va_to_pa(r);
	}

	return addr;
}

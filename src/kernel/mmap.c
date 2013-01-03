/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include "include/mmap.h"
#include "include/chal.h"

static struct cos_page cos_pages[COS_MAX_MEMORY];

void cos_init_memory(void) 
{
	int i;

	for (i = 0 ; i < COS_MAX_MEMORY ; i++) {
		void *r = chal_alloc_page();
		if (NULL == r) {
			printk("cos: ERROR -- could not allocate page for cos memory\n");
		}
		cos_pages[i].addr = (paddr_t)chal_va2pa(r);
	}

	return;
}

void cos_shutdown_memory(void)
{
	int i;

	for (i = 0 ; i < COS_MAX_MEMORY ; i++) {
		paddr_t addr = cos_pages[i].addr;
		chal_free_page(chal_pa2va((void*)addr));
		cos_pages[i].addr = 0;
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
	assert(addr);

	return addr;
}

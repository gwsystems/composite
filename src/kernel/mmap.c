/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include "include/mmap.h"
#include "include/chal.h"
#include "include/shared/cos_config.h"

//#define USE_LINUX_MEM

#ifdef USE_LINUX_MEM
static struct cos_page cos_pages[COS_MAX_MEMORY];
#endif
static struct cos_page cos_kernel_pages[COS_KERNEL_MEMORY];

int cos_init_memory(void) 
{
	int i;
#ifdef USE_LINUX_MEM
	for (i = 0 ; i < COS_MAX_MEMORY ; i++) {
		void *r = chal_alloc_page();
		if (NULL == r) {
			printk("cos: ERROR -- could not allocate page for cos memory\n");
			return -1;
		}
		cos_pages[i].addr = (paddr_t)chal_va2pa(r);
	}
#endif
	for (i = 0 ; i < COS_KERNEL_MEMORY ; i++) {
		void *r = chal_alloc_page();
		if (NULL == r) {
			printk("cos: ERROR -- could not allocate page for cos kernel memory\n");
			return -1;
		}
		cos_kernel_pages[i].addr = (paddr_t)chal_va2pa(r);
	}

	return 0;
}

void cos_shutdown_memory(void)
{
	int i;
#ifdef USE_LINUX_MEM
	for (i = 0 ; i < COS_MAX_MEMORY ; i++) {
		paddr_t addr = cos_pages[i].addr;
		chal_free_page(chal_pa2va((void*)addr));
		cos_pages[i].addr = 0;
	}
#endif
	for (i = 0 ; i < COS_KERNEL_MEMORY ; i++) {
		paddr_t addr = cos_kernel_pages[i].addr;
		chal_free_page(chal_pa2va((void*)addr));
		cos_kernel_pages[i].addr = 0;
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
#ifdef USE_LINUX_MEM
	int i;
	for (i = 0 ; i < COS_MAX_MEMORY ; i++) {
		if (cos_pages[i].addr == pa) {
			return i;
		}
	}

	return 0;
#endif
	assert(pa >= COS_MEM_START);
	return ((pa - COS_MEM_START) / (PAGE_SIZE));
}

int cos_kernel_paddr_to_cap(paddr_t pa)
{
	int i;

	for (i = 0 ; i < COS_KERNEL_MEMORY ; i++) {
		if (cos_kernel_pages[i].addr == pa) {
			return i;
		}
	}

	return 0;
}   

paddr_t cos_access_page(unsigned long cap_no)
{
	paddr_t addr;

	if (cap_no >= COS_MAX_MEMORY) return 0;

#ifdef USE_LINUX_MEM
	addr = cos_pages[cap_no].addr;
#else
	addr = COS_MEM_START + cap_no * PAGE_SIZE;
#endif
	assert(addr);

	return addr;
}

paddr_t cos_access_kernel_page(unsigned long cap_no)
{
	paddr_t addr;

	if (cap_no > COS_KERNEL_MEMORY) return 0;
	addr = cos_kernel_pages[cap_no].addr;
	assert(addr);

	return addr;
}

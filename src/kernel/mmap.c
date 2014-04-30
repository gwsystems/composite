/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include "include/mmap.h"
#include "include/chal.h"
#include "include/shared/cos_config.h"

// not used for now. 
/* static struct cos_page cos_pages[COS_MAX_MEMORY]; */
/* static struct cos_page cos_kernel_pages[COS_KERNEL_MEMORY]; */
static void *kmem_start;
static paddr_t kmem_start_pa;

int cos_init_memory(void) 
{
	kmem_start = chal_alloc_kern_mem(KERN_MEM_ORDER);
	if (!kmem_start) {
		printk("cos: ERROR -- could not allocate page for cos kernel memory\n");
		return -1;
	}

	kmem_start_pa = (paddr_t)chal_va2pa(kmem_start);

	return 0;
}

void cos_shutdown_memory(void)
{
	chal_free_kern_mem(kmem_start, KERN_MEM_ORDER);
}

int cos_paddr_to_cap(paddr_t pa)
{
	assert(pa >= COS_MEM_START && pa < (COS_MEM_START + COS_MAX_MEMORY*PAGE_SIZE));
	return ((pa - COS_MEM_START) / (PAGE_SIZE));
}

int cos_kernel_paddr_to_cap(paddr_t pa)
{
	assert(pa >= kmem_start_pa && pa < (kmem_start_pa + COS_KERNEL_MEMORY*PAGE_SIZE));
	return ((pa - kmem_start_pa) / (PAGE_SIZE));
}   

paddr_t cos_access_page(unsigned long cap_no)
{
	paddr_t addr;

	if (cap_no >= COS_MAX_MEMORY) return 0;

	addr = COS_MEM_START + cap_no * PAGE_SIZE;
	assert(addr);

	return addr;
}

paddr_t cos_access_kernel_page(unsigned long cap_no)
{
	paddr_t addr;

	if (cap_no > COS_KERNEL_MEMORY) return 0;
	addr = kmem_start_pa + cap_no * PAGE_SIZE;
	assert(addr);

	return addr;
}

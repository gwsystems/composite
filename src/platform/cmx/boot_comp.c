#include "assert.h"
#include "kernel.h"
#include "boot_comp.h"
#include "chal_cpu.h"
#include "mem_layout.h"
#include "string.h"
#include <pgtbl.h>
#include <thd.h>
#include <component.h>
#include <inv.h>
#include <hw.h>

extern void comp1(void);

void
kern_boot_comp(void)
{

	int ret = 0, nkmemptes;
	struct captbl *ct;
	unsigned int i;
	u8_t *boot_comp_captbl;
	void *thd_mem, *tcap_mem;
	pgtbl_t pgtbl   = (pgtbl_t)chal_va2pa(&boot_comp_pgd), boot_vm_pgd;
	u32_t hw_bitmap = 0xFFFFFFFF;

	printk("Setting up the booter component.\n");

	boot_comp_captbl = mem_boot_alloc(BOOT_CAPTBL_NPAGES);
	assert(boot_comp_captbl);
	ct               = captbl_create(boot_comp_captbl);
	assert(ct);

	/* expand the captbl to use multiple pages. */
	for (i = PAGE_SIZE ; i < BOOT_CAPTBL_NPAGES*PAGE_SIZE ; i += PAGE_SIZE) {
		captbl_init(boot_comp_captbl + i, 1);
		ret = captbl_expand(ct, (i - PAGE_SIZE/2)/CAPTBL_LEAFSZ, captbl_maxdepth(), boot_comp_captbl + i);
		assert(!ret);
		captbl_init(boot_comp_captbl + PAGE_SIZE + PAGE_SIZE/2, 1);
		ret = captbl_expand(ct, i/CAPTBL_LEAFSZ, captbl_maxdepth(), boot_comp_captbl + i + PAGE_SIZE/2);
		assert(!ret);
	}

	thd_mem  = mem_boot_alloc(1);
	tcap_mem = mem_boot_alloc(1);
	assert(thd_mem && tcap_mem);

	if (captbl_activate_boot(ct, BOOT_CAPTBL_SELF_CT)) assert(0);
	if (sret_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SRET)) assert(0);

	hw_asndcap_init();
	if (hw_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_INITHW_BASE, hw_bitmap)) assert(0);

	/*
	 * separate pgd for boot component virtual memory
	 */
	boot_vm_pgd = (pgtbl_t)mem_boot_alloc(1);
	assert(boot_vm_pgd);
	memcpy((void *)boot_vm_pgd + KERNEL_PGD_REGION_OFFSET,  (void *)(&boot_comp_pgd) + KERNEL_PGD_REGION_OFFSET, KERNEL_PGD_REGION_SIZE);
	if (pgtbl_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_PT, (pgtbl_t)chal_va2pa(boot_vm_pgd), 0)) assert(0);

	ret = boot_pgtbl_mappings_add(ct, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_BOOTVM_PTE, "booter VM", mem_bootc_start(),
				      (unsigned long)mem_bootc_vaddr(), mem_bootc_end() - mem_bootc_start(), 1);
	assert(ret == 0);

	/*
	 * This _must_ be the last allocation.  The bump pointer
	 * modifies this allocation.
	 *
	 * Need to account for the pages that will be allocated as
	 * PTEs
	 */
	if (pgtbl_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_UNTYPED_PT, pgtbl, 0)) assert(0);
	nkmemptes = boot_nptes(mem_utmem_end() - mem_boot_end());
	ret = boot_pgtbl_mappings_add(ct, BOOT_CAPTBL_SELF_UNTYPED_PT, BOOT_CAPTBL_KM_PTE, "untyped memory", mem_boot_nalloc_end(nkmemptes),
				      BOOT_MEM_KM_BASE, mem_utmem_end() - mem_boot_nalloc_end(nkmemptes), 0);
	assert(ret == 0);

	printk("\tCapability table and page-table created.\n");

	/* Shut off further bump allocations */
	glb_memlayout.allocs_avail = 0;

	if (comp_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP, BOOT_CAPTBL_SELF_CT,
			  BOOT_CAPTBL_SELF_PT, 0, (vaddr_t)mem_bootc_entry(), NULL)) assert(0);
	printk("\tCreated boot component structure from page-table and capability-table.\n");

	kern_boot_thd(ct, thd_mem, tcap_mem);

	printk("\tBoot component initialization complete.\n");
}


void
kern_boot_upcall(void)
{
	//u8_t *entry = mem_bootc_entry();
	u32_t flags = 0;
	void *p;

	//printk("Upcall into boot component at ip 0x%x\n", entry);
	printk("------------------[ Kernel boot complete ]------------------\n");

	chal_user_upcall(comp1, thd_current(cos_cpu_local_info())->tid);
	assert(0);		/* should never get here! */
}


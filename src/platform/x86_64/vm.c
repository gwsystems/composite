#include <kernel.h>
#include <compiler.h>
#include <pgtbl.h>
#include <thread.h>

#include <chal_cpu.h>
#include <chal_pgtbl.h>
#include <consts.h>
#include <arch_consts.h>
#include <types.h>
#include <cos_bitmath.h>

#define KERN_INIT_PGD_IDX ((COS_MEM_KERN_START_VA & COS_MEM_KERN_HIGH_ADDR_VA_PGD_MASK) >> (PGD_SHIFT))

u64_t boot_comp_pgt1[PGT1_PER_PTBL] COS_PAGE_ALIGNED = {0};
u64_t boot_comp_pgd[PGD_PER_PTBL] COS_PAGE_ALIGNED = {0};

void *
device_pa2va(paddr_t dev_addr)
{
	return chal_pa2va(dev_addr);
}

void *
device_map_mem(paddr_t dev_addr, unsigned int pt_extra_flags)
{
	return device_pa2va(dev_addr);
}


void
kern_paging_map_init(void)
{
	u64_t i = 0;

	/* lower mapping so that physical addresses work during boot, removed later */
	boot_comp_pgd[0] = (u64_t)chal_va2pa(&boot_comp_pgt1) | X86_PGTBL_PRESENT | X86_PGTBL_WRITABLE;
	/* higher mapping post-boot */
	boot_comp_pgd[KERN_INIT_PGD_IDX] = (u64_t)chal_va2pa(&boot_comp_pgt1) | X86_PGTBL_PRESENT | X86_PGTBL_WRITABLE;

	/* Map in the first 512 GB (which might extend beyond physical memory) */
	for (i = 0; i < PGT1_PER_PTBL; i++) {
		boot_comp_pgt1[i] = (i * PGT1_RANGE) | X86_PGTBL_PRESENT | X86_PGTBL_WRITABLE | X86_PGTBL_SUPER | X86_PGTBL_GLOBAL;
	}
}

void
paging_init(void)
{
	printk("Initializing virtual memory\n");
	printk("\tSetting up initial page directory.\n");

	/* unmap lower addresses that are only needed at boot-time */
	boot_comp_pgd[0] = 0;
 	/* simple, incomplete sanity checks that the page-table is still properly formatted */
	assert(boot_comp_pgd[KERN_INIT_PGD_IDX] == ((u64_t)chal_va2pa(&boot_comp_pgt1) | X86_PGTBL_PRESENT | X86_PGTBL_WRITABLE));
	assert(boot_comp_pgt1[0] = (X86_PGTBL_PRESENT | X86_PGTBL_WRITABLE | X86_PGTBL_SUPER | X86_PGTBL_GLOBAL));
	assert(boot_comp_pgt1[1] = (PGT1_RANGE | X86_PGTBL_PRESENT | X86_PGTBL_WRITABLE | X86_PGTBL_SUPER | X86_PGTBL_GLOBAL));
	printk("\tLoading the page-table.\n");
	chal_cpu_pgtbl_activate((pgtbl_t)chal_va2pa(&boot_comp_pgd));
}

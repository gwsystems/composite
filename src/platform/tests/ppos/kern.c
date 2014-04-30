#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#include "include/shared/cos_types.h"
#include <captbl.h>
#include <pgtbl.h>
#include <cap_ops.h>
#include "include/thread.h"
#include <component.h>
#include <inv.h>

u8_t boot_comp_captbl[PAGE_SIZE] PAGE_ALIGNED;
u8_t boot_comp_pgd[PAGE_SIZE]    PAGE_ALIGNED;
u8_t boot_comp_pte_vm[PAGE_SIZE] PAGE_ALIGNED;
u8_t boot_comp_pte_pm[PAGE_SIZE] PAGE_ALIGNED;

unsigned long sys_maxmem      = 1<<10; /* 4M of physical memory (2^10 pages) */
unsigned long sys_llbooter_sz = 10;    /* how many pages is the llbooter? */

/* 
 * Initial captbl setup:  
 * 0 = sret, 
 * 1 = this captbl, 
 * 2 = our pgtbl root,
 * 3 = empty
 * 4-5 = our component,
 * 6-7 = empty
 * 8 = vm pte for booter
 * 9 = vm pte for physical memory
 * 
 * Initial pgtbl setup (addresses):
 * 4MB-> = boot component VM
 * 1GB-> = system physical memory
 */
enum {
	BOOT_CAPTBL_SRET = 0, 
	BOOT_CAPTBL_SELF_CT = 1, 
	BOOT_CAPTBL_SELF_PT = 2, 
	BOOT_CAPTBL_SELF_COMP = 4, 
	BOOT_CAPTBL_BOOTVM_PTE = 8, 
	BOOT_CAPTBL_PHYSM_PTE = 9, 
};
enum {
	BOOT_MEM_VM_BASE = 1<<22,
	BOOT_MEM_PM_BASE = 1<<30,
};

void
kern_boot_comp(void)
{
	struct captbl *ct;
	pgtbl_t pt;
	unsigned int i;

	ct = captbl_create(boot_comp_captbl);
	assert(ct);
	pt = pgtbl_create(boot_comp_pgd);
	assert(pt);
	pgtbl_init_pte(boot_comp_pte_vm);
	pgtbl_init_pte(boot_comp_pte_pm);

	assert(!captbl_activate_boot(ct, BOOT_CAPTBL_SELF_CT));
	assert(!sret_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SRET));
	assert(!pgtbl_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_PT, pt, 0));
	assert(!pgtbl_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_BOOTVM_PTE, (pgtbl_t)boot_comp_pte_vm, 1));
	assert(!pgtbl_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_PHYSM_PTE, (pgtbl_t)boot_comp_pte_pm, 1));
	assert(!comp_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP, 
			      BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_PT, 0, 0x37337, NULL));

	/* construct the page tables */
	assert(!cap_cons(ct, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_BOOTVM_PTE, BOOT_MEM_VM_BASE));
	assert(!cap_cons(ct, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_PHYSM_PTE, BOOT_MEM_PM_BASE));
	/* add the component's virtual memory at 4MB (1<<22) using "physical memory" starting at 0xADEAD000 */
	for (i = 0 ; i < sys_llbooter_sz ; i++) {
		u32_t addr = 0xADEAD000 + i*PAGE_SIZE;
		u32_t flags;
		assert(!cap_memactivate(ct, BOOT_CAPTBL_SELF_PT, 
					BOOT_MEM_VM_BASE + i*PAGE_SIZE, 
					addr, PGTBL_USER_DEF));
		assert(chal_pa2va((void *)addr) == pgtbl_lkup(pt, BOOT_MEM_VM_BASE+i*PAGE_SIZE, &flags));
	}
	/* add the system's physical memory at address 1GB */
	for (i = 0 ; i < sys_maxmem ; i++) {
		u32_t addr = i*PAGE_SIZE;
		u32_t flags;
		assert(!cap_memactivate(ct, BOOT_CAPTBL_SELF_PT, 
					BOOT_MEM_PM_BASE + i*PAGE_SIZE, 
					addr, PGTBL_COSFRAME));
		assert(chal_pa2va((void *)addr) == pgtbl_lkup(pt, BOOT_MEM_PM_BASE+i*PAGE_SIZE, &flags));
	}
}

void 
kern_main(void)
{
	cap_init();
	ltbl_init();
	comp_init();
	thd_init();
	inv_init();
	kern_boot_comp();
}

int main(void) { kern_main(); return 0; }

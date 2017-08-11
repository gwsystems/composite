#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#include "include/shared/cos_types.h"

#include "captbl.h"
#include "pgtbl.h"
#include "cap_ops.h"
#include "include/thd.h"
#include "include/cpuid.h"
#include "component.h"
#include "inv.h"

u32_t free_thd_id;

u8_t c0_comp_captbl[PAGE_SIZE] PAGE_ALIGNED;
u8_t boot_comp_captbl[PAGE_SIZE] PAGE_ALIGNED;
u8_t boot_comp_pgd[PAGE_SIZE] PAGE_ALIGNED;
u8_t boot_comp_pte_vm[PAGE_SIZE] PAGE_ALIGNED;
u8_t boot_comp_pte_pm[PAGE_SIZE] PAGE_ALIGNED;
u8_t thdinit[PAGE_SIZE] PAGE_ALIGNED;

unsigned long sys_maxmem      = 1 << 10; /* 4M of physical memory (2^10 pages) */
unsigned long sys_llbooter_sz = 10;      /* how many pages is the llbooter? */

struct thread *__thd_current;
unsigned long  __cr3_contents;

int
printfn(struct pt_regs *regs)
{
	(void)(regs);
	return 0;
}


/*
 * Initial captbl setup:
 * 0 = sret,
 * 1 = this captbl,
 * 2 = our pgtbl root,
 * 3 = initial thread,
 * 4-5 = our component,
 * 6-7 = nil,
 * 8 = vm pte for booter
 * 9 = vm pte for physical memory
 * 10-11 = nil,
 * 12 = comp0 captbl,
 * 13 = comp0 pgtbl root,
 * 14-15 = nil,
 * 16-17 = comp0 component,
 *
 * Initial pgtbl setup (addresses):
 * 4MB-> = boot component VM
 * 1GB-> = system physical memory
 */
/* enum { */
/* 	BOOT_CAPTBL_SRET = 0,  */
/* 	BOOT_CAPTBL_SELF_CT = 1,  */
/* 	BOOT_CAPTBL_SELF_PT = 2,  */
/* 	BOOT_CAPTBL_SELF_INITTHD = 3,  */
/* 	BOOT_CAPTBL_SELF_COMP = 4,  */
/* 	BOOT_CAPTBL_BOOTVM_PTE = 8,  */
/* 	BOOT_CAPTBL_PHYSM_PTE = 9,  */

/* 	BOOT_CAPTBL_COMP0_CT = 12, */
/* 	BOOT_CAPTBL_COMP0_PT = 13,   */
/* 	BOOT_CAPTBL_COMP0_COMP = 16,  */
/* }; */
/* enum { */
/* 	BOOT_MEM_VM_BASE = 1<<22, */
/* 	BOOT_MEM_PM_BASE = 1<<30, */
/* }; */

int syscall_handler(struct pt_regs *regs);

void
kern_boot_comp(void)
{
	struct pt_regs regs;
	struct captbl *ct, *ct0;
	pgtbl_t        pt, pt0 = 0;
	unsigned int   i;
	struct thread *thd = (struct thread *)thdinit;

	/* llbooter's captbl */
	ct = captbl_create(boot_comp_captbl);
	assert(ct);
	pt = pgtbl_create(boot_comp_pgd, boot_comp_pgd);
	assert(pt);
	pgtbl_init_pte(boot_comp_pte_vm);
	pgtbl_init_pte(boot_comp_pte_pm);

	assert(!captbl_activate_boot(ct, BOOT_CAPTBL_SELF_CT));
	assert(!sret_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SRET));
	assert(!pgtbl_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_PT, pt, 0));
	assert(!pgtbl_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_BOOTVM_PTE, (pgtbl_t)boot_comp_pte_vm, 1));
	assert(!pgtbl_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_PHYSM_PTE, (pgtbl_t)boot_comp_pte_pm, 1));
	assert(!comp_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_PT,
	                      0, 0x37337, NULL));
	/* construct the page tables */
	assert(!cap_cons(ct, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_BOOTVM_PTE, BOOT_MEM_VM_BASE));
	assert(!cap_cons(ct, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_PHYSM_PTE, BOOT_MEM_PM_BASE));
	/* add the component's virtual memory at 4MB (1<<22) using "physical memory" starting at 0xADEAD000 */
	for (i = 0; i < sys_llbooter_sz; i++) {
		u32_t addr = 0xADEAD000 + i * PAGE_SIZE;
		u32_t flags;
		assert(
		  !cap_memactivate(ct, BOOT_CAPTBL_SELF_PT, BOOT_MEM_VM_BASE + i * PAGE_SIZE, addr, PGTBL_USER_DEF));
		assert(chal_pa2va((void *)addr) == pgtbl_lkup(pt, BOOT_MEM_VM_BASE + i * PAGE_SIZE, &flags));
	}
	/* add the system's physical memory at address 1GB */
	for (i = 0; i < sys_maxmem; i++) {
		u32_t addr = i * PAGE_SIZE;
		u32_t flags;
		assert(
		  !cap_memactivate(ct, BOOT_CAPTBL_SELF_PT, BOOT_MEM_PM_BASE + i * PAGE_SIZE, addr, PGTBL_COSFRAME));
		assert(chal_pa2va((void *)addr) == pgtbl_lkup(pt, BOOT_MEM_PM_BASE + i * PAGE_SIZE, &flags));
	}

	/* comp0's data, culminated in a static invocation capability to the llbooter */
	ct0 = captbl_create(c0_comp_captbl);
	assert(ct0);
	assert(!captbl_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_COMP0_CT, ct0, 0));
	/* pt0 should be replaced with page tables from the Linux cos_loader */
	assert(!pgtbl_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_COMP0_PT, pt0, 0));
	assert(!comp_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_COMP0_COMP, BOOT_CAPTBL_COMP0_CT,
	                      BOOT_CAPTBL_COMP0_PT, 0, 0x37337, NULL));

	/*
	 * Only capability for the comp0 is 0: the synchronous
	 * invocation capability.
	 *
	 * Replace 0xADD44343 with the actual entry-point in the
	 * llbooter!
	 */
	assert(!sinv_activate(ct, BOOT_CAPTBL_COMP0_CT, 0, BOOT_CAPTBL_SELF_COMP, 0xADD44343));

	/*
	 * Create a thread in comp0.
	 */
	assert(!thd_activate(ct, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_INITTHD_BASE, thd, BOOT_CAPTBL_COMP0_COMP, 0));
	thd_current_update(thd, NULL);

	/*
	 * Synchronous invocation!
	 */
	u32_t cap_no   = 1;
	u32_t ret_cap  = 0;
	u32_t orig_cr3 = __cr3_contents;
	regs.ax        = (cap_no + 1) << COS_CAPABILITY_OFFSET; /* sinv */
	syscall_handler(&regs);
	assert(cos_cpu_local_info()->invstk_top > 0); /* we cache invstk_top on kernel stk */
	assert(__cr3_contents != orig_cr3);
	regs.ax = (ret_cap + 1) << COS_CAPABILITY_OFFSET; /* sret */
	syscall_handler(&regs);
	assert(cos_cpu_local_info()->invstk_top == 0);
	assert(__cr3_contents == orig_cr3);
	printf("Test passed!\n");
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

int
main(void)
{
	kern_main();
	return 0;
}

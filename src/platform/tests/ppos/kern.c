#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <assert.h>

#ifndef unlikely
#define unlikely(x)     __builtin_expect(!!(x), 0)
#endif  unlikely

#include <cos_types.h>
#include <captbl.h>
#include <pgtbl.h>
#include <liveness_tbl.h>
#include <thread.h>
#include <component.h>
#include <inv.h>

u8_t boot_comp_captbl[PAGE_SIZE] PAGE_ALIGNED;
u8_t boot_comp_pgd[PAGE_SIZE]    PAGE_ALIGNED;
u8_t boot_comp_pte_vm[PAGE_SIZE] PAGE_ALIGNED;
u8_t boot_comp_pte_pm[PAGE_SIZE] PAGE_ALIGNED;

unsigned long sys_maxmem      = 1<<10; /* 4M of physical memory (2^10 pages) */
unsigned long sys_llbooter_sz = 10;    /* how many pages is the llbooter? */

void
kern_boot_comp(void)
{
	struct captbl *ct;
	pgtbl_t pt;
	int i;

	ct = captbl_create(boot_comp_captbl);
	assert(ct);
	pt = pgtbl_create(boot_comp_pgd);
	pgtbl_init_pte(boot_comp_pte_vm);
	pgtbl_init_pte(boot_comp_pte_pm);
	/* Virtual memory at 4MB */
	if (pgtbl_intern_expand(pt, (void *)(1<<22), boot_comp_pte, PGTBL_INTERN_DEF)) assert(0);
	assert(sys_llbooter_sz <= 1<<10); /* need another pte otherwise */
	for (i = 0 ; i < sys_llbooter_sz ; i++) {
		if (pgtbl_mapping_add(pt, (void *)((1<<22) + PAGE_SIZE * i), (void *)0xADEAD000, 
				      PGTBL_PRESENT | PGTBL_USER | PGTBL_WRITABLE)) assert(0);
	}
	/* Physical memory starting at 1GB */
	if (pgtbl_intern_expand(pt, (void *)(1<<30), boot_comp_pte, PGTBL_INTERN_DEF)) assert(0);
	assert(sys_maxmem <= 1<<10); /* need another pte otherwise */
	for (i = 0 ; i < sys_maxmem ; i++) {
 		if (pgtbl_mapping_add(pt, (void *)((1<<30) + (PAGE_SIZE * i)), 
				      (void *)(i + PAGE_SIZE), PGTBL_COSFRAME)) assert(0);
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

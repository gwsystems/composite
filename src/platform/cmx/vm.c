#include <cc.h>
#include <pgtbl.h>
#include <thd.h>

#include "kernel.h"
#include "string.h"
//#include "chal_cpu.h"

struct tlb_quiescence tlb_quiescence[NUM_CPU]   CACHE_ALIGNED;
struct liveness_entry __liveness_tbl[LTBL_ENTS] CACHE_ALIGNED;

#define KERN_INIT_PGD_IDX (COS_MEM_KERN_START_VA>>PGD_SHIFT)
u32_t boot_comp_pgd[PAGE_SIZE/sizeof(u32_t)] PAGE_ALIGNED = {
	[0]                 = 0 | PGTBL_PRESENT | PGTBL_WRITABLE | PGTBL_SUPER,
	[KERN_INIT_PGD_IDX] = 0 | PGTBL_PRESENT | PGTBL_WRITABLE | PGTBL_SUPER
};

void
kern_retype_initial(void)
{

}

u8_t *
mem_boot_alloc(int npages) /* boot-time, bump-ptr heap */
{
	return 0;
}

int
kern_setup_image(void)
{
	chal_cpu_init();
	chal_cpu_pgtbl_activate((pgtbl_t)chal_va2pa(boot_comp_pgd));

	kern_retype_initial();

	return 0;
}

void
kern_paging_map_init(void *pa)
{
	unsigned long i, j;
	paddr_t kern_pa_start = 0, kern_pa_end = (paddr_t)pa;

	for (i = kern_pa_start, j = COS_MEM_KERN_START_VA/PGD_RANGE ;
	     i < (unsigned long)round_up_to_pgd_page(kern_pa_end) ;
	     i += PGD_RANGE, j++) {
		assert(j != KERN_INIT_PGD_IDX ||
		       ((boot_comp_pgd[j] | PGTBL_GLOBAL) & ~(PGTBL_MODIFIED | PGTBL_ACCESSED)) ==
		       (i | PGTBL_PRESENT | PGTBL_WRITABLE | PGTBL_SUPER | PGTBL_GLOBAL));
		/* lower mapping */
		boot_comp_pgd[i/PGD_RANGE] = i | PGTBL_PRESENT | PGTBL_WRITABLE | PGTBL_SUPER | PGTBL_GLOBAL;
		/* higher mapping */
		boot_comp_pgd[j] = i | PGTBL_PRESENT | PGTBL_WRITABLE | PGTBL_SUPER | PGTBL_GLOBAL;
	}
}


void
paging_init(void)
{
	int ret;

	printk("Initializing virtual memory\n");
	if ((ret = kern_setup_image())) {
		die("Could not set up kernel image, errno %d.\n", ret);
	}
}

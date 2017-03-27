#include <cc.h>
#include <pgtbl.h>
#include <thd.h>

#include "kernel.h"
#include "string.h"
#include "chal_cpu.h"
#include "mem_layout.h"

struct tlb_quiescence tlb_quiescence[NUM_CPU]   CACHE_ALIGNED;
struct liveness_entry __liveness_tbl[LTBL_ENTS] CACHE_ALIGNED;

#define KERN_INIT_PGD_IDX (COS_MEM_KERN_START_VA>>PGD_SHIFT)
/* This secondary page table at 0x20030000
u32_t boot_comp_pte[1024] PAGE_ALIGNED = {
		[0] = 0x20030<<12
		[1] = 0x20031
		[2] = 0x20032
		[3] = 0x20033
		[4] = 0x20034
		[5] = 0x20035
		[6] = 0x20036
		[7] = 0x20037
		[8] = 0x20038
		[9] = 0x20039
		[10] = 0x2003A
		[11] = 0x2003B
		[12] = 0x2003C
		[13] = 0x2003D
		[14] = 0x2003E
		[15] = 0x2003F
};*/
u32_t boot_comp_pgd[PAGE_SIZE/sizeof(u32_t)] PAGE_ALIGNED = {
	[0]                 = 0 | PGTBL_PRESENT | PGTBL_WRITABLE | PGTBL_SUPER//,
	//[KERN_INIT_PGD_IDX] = 0 | PGTBL_PRESENT | PGTBL_WRITABLE | PGTBL_SUPER
};

void
kern_retype_initial(void)
{
	u8_t *k;

	assert((int)mem_bootc_start() % RETYPE_MEM_NPAGES == 0);
	assert((int)mem_bootc_end()   % RETYPE_MEM_NPAGES == 0);
	for (k = mem_bootc_start() ; k < mem_bootc_end() ; k += PAGE_SIZE * RETYPE_MEM_NPAGES) {
		if (retypetbl_retype2user((void*)(chal_va2pa(k)))) assert(0);
	}
}

//void
//kern_paging_map_init(void *pa)
//{
//	unsigned long i, j;
//	paddr_t kern_pa_start = 0, kern_pa_end = (paddr_t)pa;
//
//	for (i = kern_pa_start, j = COS_MEM_KERN_START_VA/PGD_RANGE ;
//	     i < (unsigned long)round_up_to_pgd_page(kern_pa_end) ;
//	     i += PGD_RANGE, j++) {
//		assert(j != KERN_INIT_PGD_IDX ||
//		       ((boot_comp_pgd[j] | PGTBL_GLOBAL) & ~(PGTBL_MODIFIED | PGTBL_ACCESSED)) ==
//		       (i | PGTBL_PRESENT | PGTBL_WRITABLE | PGTBL_SUPER | PGTBL_GLOBAL));
//		/* lower mapping */
//		boot_comp_pgd[i/PGD_RANGE] = i | PGTBL_PRESENT | PGTBL_WRITABLE | PGTBL_SUPER | PGTBL_GLOBAL;
//		/* higher mapping */
//		boot_comp_pgd[j] = i | PGTBL_PRESENT | PGTBL_WRITABLE | PGTBL_SUPER | PGTBL_GLOBAL;
//	}
//}

u8_t *
mem_boot_alloc(int npages) /* boot-time, bump-ptr heap */
{
	u8_t *r = glb_memlayout.kern_boot_heap;
	unsigned long i;

	assert(glb_memlayout.allocs_avail);

	glb_memlayout.kern_boot_heap += npages * (PAGE_SIZE/sizeof(u8_t));
	assert(glb_memlayout.kern_boot_heap <= mem_kmem_end());
	for (i = (unsigned long)r ; i < (unsigned long)glb_memlayout.kern_boot_heap ; i += PAGE_SIZE) {
		if ((unsigned long)i % RETYPE_MEM_NPAGES == 0) {
			if (retypetbl_retype2kern((void*)chal_va2pa((void*)i))) {
				die("Retyping to kernel on boot-time heap allocation failed @ 0x%x.\n", i);
			}
		}
	}

	memset((void *)r, 0, npages * (PAGE_SIZE/sizeof(u8_t)));

	return r;
}

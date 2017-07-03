#include <cc.h>
#include <pgtbl.h>
#include <thd.h>

#include "kernel.h"
#include "string.h"
#include "chal_cpu.h"
#include "mem_layout.h"

struct liveness_entry __liveness_tbl[LTBL_ENTS] CACHE_ALIGNED;

#define KERN_INIT_PGD_IDX (COS_MEM_KERN_START_VA>>PGD_SHIFT)

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

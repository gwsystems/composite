#include <cc.h>
#include <pgtbl.h>
#include <thd.h>

#include "kernel.h"
#include "string.h"
#include "chal_cpu.h"
#include "mem_layout.h"
#include "chal_pgtbl.h"

struct tlb_quiescence tlb_quiescence[NUM_CPU] CACHE_ALIGNED;
struct liveness_entry __liveness_tbl[LTBL_ENTS] CACHE_ALIGNED;

#define KERN_INIT_PGD_IDX (COS_MEM_KERN_START_VA >> PGD_SHIFT)

/* We are not using this, and we will not support AP */
u32_t boot_comp_pgd[PAGE_SIZE / sizeof(u32_t)] PAGE_ALIGNED = {0};

void
kern_retype_initial(void)
{
	u8_t *k;

	assert((int)mem_bootc_start() % RETYPE_MEM_NPAGES == 0);
	assert((int)mem_bootc_end() % RETYPE_MEM_NPAGES == 0);
	printk("PA-VA: 0x%x\r\n", COS_MEM_KERN_START_VA);
	for (k = mem_bootc_start(); k < mem_bootc_end(); k += PAGE_SIZE * RETYPE_MEM_NPAGES) {
		if (retypetbl_retype2user((void *)chal_va2pa(k), PAGE_ORDER)) assert(0);
	}
}

u8_t *
mem_boot_user_alloc(int npages) /* boot-time, bump-ptr heap */
{
	u8_t *        r = glb_memlayout.kern_boot_heap;
	unsigned long i;

	assert(glb_memlayout.allocs_avail);

	glb_memlayout.kern_boot_heap += npages * (PAGE_SIZE / sizeof(u8_t));
	assert(glb_memlayout.kern_boot_heap <= mem_kmem_end());
	for (i = (unsigned long)r; i < (unsigned long)glb_memlayout.kern_boot_heap; i += PAGE_SIZE) {
		if ((unsigned long)i % RETYPE_MEM_NPAGES == 0) {
			if (retypetbl_retype2user((void *)chal_va2pa((void *)i), PAGE_ORDER)) {}
		}
	}

	memset((void *)r, 0, npages * (PAGE_SIZE / sizeof(u8_t)));

	return r;
}

u8_t *
mem_boot_alloc(int npages) /* boot-time, bump-ptr heap */
{
	u8_t *        r = glb_memlayout.kern_boot_heap;
	unsigned long i;

	assert(glb_memlayout.allocs_avail);

	glb_memlayout.kern_boot_heap += npages * (PAGE_SIZE / sizeof(u8_t));
	assert(glb_memlayout.kern_boot_heap <= mem_kmem_end());
	for (i = (unsigned long)r; i < (unsigned long)glb_memlayout.kern_boot_heap; i += PAGE_SIZE) {
		if ((unsigned long)i % RETYPE_MEM_NPAGES == 0) {
			if (retypetbl_retype2kern((void *)chal_va2pa((void *)i), PAGE_ORDER)) {}
		}
	}

	memset((void *)r, 0, npages * (PAGE_SIZE / sizeof(u8_t)));

	return r;
}

int
kern_setup_image(void)
{
	chal_cpu_init();

	kern_retype_initial();

	return 0;
}

#define SCLR_UNLOCK_REG (*((volatile unsigned long *)0xF8000008))
#define SCLR_LOCK_REG (*((volatile unsigned long *)0xF8000004))
#define OCM_MAP_REG (*((volatile unsigned long *)0xF8000910))
#define OCM_RESET_REG (*((volatile unsigned long *)0xF8000238))
#define SCU_FILTER_START (*((volatile unsigned long *)0xF8F00040))
void
kern_paging_map_init(void *pa)
{
	/* We will initialize these mappings ourselves in assembly - we are already done */
	/* is the OCM ready, or not at all? are we accessing SRAM or worse? */
	/* Unlock the SCLR */
	SCLR_UNLOCK_REG = 0xDF0D;

	/* Map OCM to higher address - might have been an issue here */
	OCM_MAP_REG = 0x10;
	/* OCM reset */
	OCM_RESET_REG = 0x01;
	printk("OCM reg: %x\n", OCM_MAP_REG);
	OCM_RESET_REG = 0x00;

	/* Relock the SCLR */
	SCLR_LOCK_REG = 0x767B;

	/* Die if we fail miserably */
	assert(*((volatile unsigned long *)0xF8000910) == 0x10);

	/* Change the filtering control so that we now go to the SRAM */
	SCU_FILTER_START = 0x00100000;
}


void
paging_init(void)
{
	int ret;

	printk("Initializing virtual memory\n");
	if ((ret = kern_setup_image())) { die("Could not set up kernel image, errno %d.\n", ret); }
}

#include <stdio.h>
#include <string.h>
#include <cos_component.h>

#include <cobj_format.h>
#include <cos_kernel_api.h>

#include "rumpcalls.h"
#include "vk_api.h"
#include "cos_sync.h"
#include "rk_json_cfg.h"

extern struct cos_compinfo *currci;
extern int vmid;

u64_t t_vm_cycs  = 0;
u64_t t_dom_cycs = 0;

static unsigned long
rk_alloc_initmem_all(void)
{
	/* RG:
	 * 1 increment is 1 page, we start at 1 as the first page is fetched
	 * from cos_run.
	 */
	int count = 1;

	/* bytes to pages, add 1 to compensate for trunctation */
	int max_rk = (RK_TOTAL_MEM / 4096) + 1;
	printc("Looking to get %d pages\n", max_rk);

	void *curpage;
	void *nxtpage = cos_page_bump_alloc(currci);
	int  *nxtpage_test = (int *)nxtpage;
	*nxtpage_test = 1;

	while(count <= max_rk && nxtpage != NULL) {
		curpage = nxtpage;
		int *curpage_test = (int *)nxtpage;
		*curpage_test = 1;

		nxtpage = cos_page_bump_alloc(currci);
		count++;
	}

	if(count < max_rk) printc("Did not allocate desired amount of mem for RK! Ran out of mem first\n");

	unsigned long max = (unsigned long)curpage;
	printc("max: %p\n", (void *)max);
	return max;
}

void
rk_hw_irq_alloc(void)
{
	tcap_res_t budget;
	int i, ret;
	int first = 1, id = HW_ISR_FIRST;

	assert(vmid == 0);
	memset(irq_thdcap, 0, sizeof(irq_thdcap));
	memset(irq_thdid, 0, sizeof(irq_thdid));
	memset(irq_arcvcap, 0, sizeof(irq_arcvcap));
	memset(irq_tcap, 0, sizeof(irq_tcap));
	memset(irq_prio, 0, sizeof(irq_prio));

	for(i = HW_ISR_FIRST; i < HW_ISR_LINES; i++){
		switch(i) {
#if defined(APP_COMM_ASYNC)
			case RK_IRQ_IO:
				intr_update(i, 0);
				irq_thdcap[i] = SUB_CAPTBL_SELF_IOTHD_BASE;
				irq_thdid[i] = (thdid_t)cos_introspect(currci, irq_thdcap[i], THD_GET_TID);
				irq_arcvcap[i] = SUB_CAPTBL_SELF_IORCV_BASE;
				irq_tcap[i] = BOOT_CAPTBL_SELF_INITTCAP_BASE;
				irq_prio[i] = PRIO_LOW;
				break;
#endif
			default:
				intr_update(i, 0);
				irq_thdcap[i] = cos_thd_alloc(currci, currci->comp_cap, cos_irqthd_handler, (void *)i);
				assert(irq_thdcap[i]);
				irq_thdid[i] = (thdid_t)cos_introspect(currci, irq_thdcap[i], THD_GET_TID);
				assert(irq_thdid[i]);
				irq_prio[i] = PRIO_MID;

				irq_tcap[i] = BOOT_CAPTBL_SELF_INITTCAP_BASE;
				irq_arcvcap[i] = cos_arcv_alloc(currci, irq_thdcap[i], BOOT_CAPTBL_SELF_INITTCAP_BASE, currci->comp_cap, BOOT_CAPTBL_SELF_INITRCV_BASE);
				assert(irq_arcvcap[i]);
				cos_hw_attach(BOOT_CAPTBL_SELF_INITHW_BASE, 32 + i, irq_arcvcap[i]);
				break;
		}
	}
}

static void
rk_alloc_run(char *cmdline)
{
	printc("\n------------------[RK]------------------\n");
	printc("Rump Kernel bootstrap on platform Composite\n");
	bmk_sched_init();
	printc("bmk_sched_init done\n");
	bmk_memalloc_init();
	printc("bmk_memalloc_init done\n");

	/*
	 * Before bmk_pgalloc_loadmem is called, I need to alloc memory till we have enough or till failure
	 * the start and end locations in memory to bmk_pgalloc
	 */
	void *minptr = cos_page_bump_alloc(currci);
	int *mintest = (int *)minptr;
	*mintest = 1;

	unsigned long min = (unsigned long)minptr;

	printc("first page: %p\n", (void *)min);
	unsigned long max = rk_alloc_initmem_all();

	// bmk_pgalloc_loadmem is needed to get the memory area from Composite
	bmk_pgalloc_loadmem(min, max);
	printc("returned from bmk_pgalloc_loadmem\n");

	bmk_sched_startmain(bmk_mainthread, cmdline);
}

void
rump_booter_init(void *d)
{
	extern int vmid;

	printc("~~~~~ vmid: %d ~~~~~\n", vmid);
	assert(vmid == 0);

	printc("\nRumpKernel Boot Start.\n");
	cos2rump_setup();

	printc("\nSetting up arcv for hw irq\n");
	rk_hw_irq_alloc();

	/* We pass in the json config string to the RK */
	rk_alloc_run(RK_JSON_DEFAULT_QEMU);
	printc("\nRumpKernel Boot done.\n");

	cos_vm_exit();
	return;
}

#include <stdio.h>
#include <string.h>
#include <cos_component.h>

#include <cobj_format.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <sl.h>
#include <sl_thd.h>
#include <sl_consts.h>

#include <rumpcalls.h>
#include <vk_types.h>
#include "rk_json_cfg.h"
#include "rk_sched.h"
#include <llprint.h>

/* TODO rumpboot component should export this when it is moved to its own interface */
//extern struct cos_compinfo *currci;
struct cos_compinfo *currci = NULL;

/*TODO same reason above */
//extern int vmid;
int vmid = 0;

u64_t t_vm_cycs  = 0;
u64_t t_dom_cycs = 0;

#define RK_TOTAL_MEM (1 << 26) //64MB
#define HW_ISR_LINES 32
#define HW_ISR_FIRST 1
#define RK_IRQ_IO 15

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
	int i;

	assert(vmid == 0);

	for(i = HW_ISR_FIRST; i < HW_ISR_LINES; i++){
		struct sl_thd *t = NULL;
		struct cos_aep_info tmpaep;

		switch(i) {
#if defined(APP_COMM_ASYNC)
			case RK_IRQ_IO:
				tmpaep.thd = SUB_CAPTBL_SELF_IOTHD_BASE;
				tmpaep.rcv = SUB_CAPTBL_SELF_IORCV_BASE;
				tmpaep.tc  = BOOT_CAPTBL_SELF_INITTCAP_BASE;
				t = rk_intr_aep_init(&tmpaep, 0);
				assert(t);
				break;
#endif
			default:
				t = rk_intr_aep_alloc(cos_irqthd_handler, (void *)i, 0);
				assert(t);
			
				cos_hw_attach(BOOT_CAPTBL_SELF_INITHW_BASE, 32 + i, sl_thd_rcvcap(t));
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
	printc("~~~~~ vmid: %d ~~~~~\n", vmid);
	assert(vmid == 0);

	printc("\nRumpKernel Boot Start.\n");
	cos2rump_setup();
	rk_sched_init(RK_SCHED_PERIOD_US);

	printc("\nSetting up arcv for hw irq\n");
	rk_hw_irq_alloc();

	/* We pass in the json config string to the RK */
	rk_alloc_run(RK_JSON_DEFAULT_QEMU);
	printc("\nRumpKernel Boot done.\n");

	cos_vm_exit();
	return;
}

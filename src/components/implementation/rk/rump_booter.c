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
#include <llprint.h>
#include <hypercall.h>
#include <memmgr.h>
#include <schedinit.h>

#include "rk_json_cfg.h"
#include "rk_sched.h"


extern struct cos_component_information cos_comp_info;

/* TODO rumpboot component should export this when it is moved to its own interface */
struct cos_compinfo *currci = NULL;

/*TODO same reason above */
//extern int vmid;
int vmid = 0;

u64_t t_vm_cycs  = 0;
u64_t t_dom_cycs = 0;

cycles_t cycs_per_usec;

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
	void *nxtpage = (void *)memmgr_heap_page_alloc();;
	int  *nxtpage_test = (int *)nxtpage;
	*nxtpage_test = 1;

	while(count <= max_rk && nxtpage != NULL) {
		curpage = nxtpage;
		int *curpage_test = (int *)nxtpage;
		*curpage_test = 1;

		nxtpage = (void *)memmgr_heap_page_alloc();;
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
	int ret;

	assert(vmid == 0);

	for (i = HW_ISR_FIRST; i < HW_ISR_LINES; i++) {
		struct sl_thd *t = NULL;
		struct cos_aep_info tmpaep;

		/* TODO, check to make sure this is the right key for the irq... */
		t = rk_intr_aep_alloc(cos_irqthd_handler, (void *)i, 0, 0);
		assert(t);

		ret = cos_hw_attach(BOOT_CAPTBL_SELF_INITHW_BASE, 32 + i, sl_thd_rcvcap(t));
		assert(!ret);
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
	void *minptr = (void *)memmgr_heap_page_alloc();
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
	char *script;
	printc("~~~~~ vmid: %d ~~~~~\n", vmid);
	assert(vmid == 0);

	printc("\nRumpKernel Boot Start.\n");
	cos2rump_setup();
	rk_sched_init(RK_SCHED_PERIOD_US);

	printc("\nSetting up arcv for hw irq\n");
	rk_hw_irq_alloc();

	printc("Notifying parent scheduler...\n");
	schedinit_child();

	/* We pass in the json config string to the RK */
	script = RK_JSON_DEFAULT_HW;
	if (!strcmp(script, RK_JSON_DEFAULT_HW)) {
		printc("CONFIGURING RK TO RUN ON BAREMETAL\n");
	} else if (!strcmp(script, RK_JSON_DEFAULT_QEMU)) {
		printc("CONFIGURING RK TO RUN ON QEMU\n");
	}
	rk_alloc_run(script);
	printc("\nRumpKernel Boot done.\n");

	cos_vm_exit();
	return;
}

capid_t udpserv_thdcap = -1;
int spdid;

void
cos_init(void)
{

	long unsigned cap_frontier = -1;
	long unsigned vas_frontier = -1;
	struct cos_defcompinfo *dci;
	struct cos_compinfo    *ci;
	struct cos_config_info_t *my_info;
	int ret = -1;

	printc("rumpkernel cos_init\n");

	dci = cos_defcompinfo_curr_get();
	assert(dci);
	ci  = cos_compinfo_get(dci);
	assert(ci);

	cos_defcompinfo_init();

	currci = ci;

	cycs_per_usec = (cycles_t)cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);

	/*
	 * We don't know how many sinv's have been allocated for us,
	 * so switch back to booter to get the frontier value
	 */

	printc("Fetching cap frontier from booter...");
	cos_spdid_set(cos_comp_info.cos_this_spd_id);
	spdid = cos_spdid_get();
	printc("spdid: %d\n", spdid);

	ret = hypercall_comp_frontier_get(cos_spdid_get(), &vas_frontier, &cap_frontier);
	assert(!ret);
	printc("done\n");

	cos_compinfo_init(ci, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
			(vaddr_t)cos_get_heap_ptr(), cap_frontier, ci);
	cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ,
			BOOT_CAPTBL_SELF_UNTYPED_PT);

	printc("Fetching boot configuration information\n");
	my_info = cos_init_args();
	printc("Greeting key: %s\n", my_info->kvp[GREETING_KEY].key);
	printc("Greeting value: %s\n", my_info->kvp[GREETING_KEY].value);

	printc("Setting up RK\n");
	rump_booter_init((void *)0);

	/* Error, should not get here */
	assert(0);
	return;
}

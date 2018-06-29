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
#include <capmgr.h>
#include <capmgr.h>

#include "rk_json_cfg.h"
#include "rk_sched.h"

extern struct cos_component_information cos_comp_info;

/* TODO rumpboot component should export this when it is moved to its own interface */
struct cos_compinfo *currci = NULL;

/*TODO same reason above */
//extern int vmid;
int vmid = 0;

cycles_t cycs_per_usec;

#define HW_ISR_LINES 32
#define HW_ISR_FIRST 1
#define RK_IRQ_IO 15

void
rk_alloc_initmem_all(vaddr_t *min, unsigned long *len)
{
	vaddr_t va = 0, pa = 0;

	va = (vaddr_t)memmgr_heap_page_allocn(RK_TOTAL_MEM / PAGE_SIZE);
	assert(va);
	memset((void *)va, 0, RK_TOTAL_MEM);

	pa = (vaddr_t)cos_vatpa((void *)va, RK_TOTAL_MEM);
	PRINTC("RK virtual memory allocated, va: %lx (pa: %lx), sz: %uMB\n", va, pa, RK_TOTAL_MEM>>20);

	*min = va;
	*len = RK_TOTAL_MEM;

	return;
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

		/*
		 * rcvcap is created by the capmgr in the capmgr component..
		 * the captbl information in the kernel will be capmgr captbl.
		 * passing the cap offset from here may very well map to some other capability in capmgr!!
		 * It must be co-incidence that it works for some cases!!
		 */
		ret = capmgr_hw_attach(32 + i, sl_thd_thdid(t));
		assert(!ret);
	}
}

static void
rk_alloc_run(char *cmdline)
{
	vaddr_t min = 0;
	unsigned long sz = 0;

	assert(cmdline);
	cache_addr_init();
	PRINTC("\n------------------[RK]------------------\n");
	PRINTC("Rump Kernel bootstrap on platform Composite\n");
	bmk_sched_init();
	PRINTC("bmk_sched_init done\n");
	bmk_memalloc_init();
	PRINTC("bmk_memalloc_init done\n");

	/*
	 * Before bmk_pgalloc_loadmem is called, I need to alloc memory till we have enough or till failure
	 * the start and end locations in memory to bmk_pgalloc
	 */
	rk_alloc_initmem_all(&min, &sz);
	assert(min && sz);

	/* bmk_pgalloc_loadmem is needed to get the memory area from Composite */
	bmk_pgalloc_loadmem(min, min + sz);
	PRINTC("returned from bmk_pgalloc_loadmem\n");

	bmk_sched_startmain(bmk_mainthread, cmdline);
}

void
rump_booter_init(void *d)
{
	char *script = NULL;
	PRINTC("~~~~~ vmid: %d ~~~~~\n", vmid);
	assert(vmid == 0);

	PRINTC("\nRumpKernel Boot Start.\n");
	cos2rump_setup();

	PRINTC("Walking through RK child components..\n");
	rk_child_initthd_walk();

	PRINTC("\nSetting up arcv for hw irq\n");
	rk_hw_irq_alloc();

	/* We pass in the json config string to the RK */
	//script = RK_JSON_HTTP_UDPSERV_QEMU;
	//script = RK_JSON_HTTP_UDPSERV_HW;
	//script = RK_JSON_UDPSTUB_HTTP_QEMU;
	//script = RK_JSON_UDPSTUB_HTTP_HW;
	//script = RK_JSON_UDPSERV_QEMU;
	//script = RK_JSON_UDPSERV_HW;
	//script = RK_JSON_IPERF_QEMU;
	//script = RK_JSON_IPERF_HW;
	//script = RK_JSON_HTTP_QEMU;
	//script = RK_JSON_HTTP_HW;
	script = RK_JSON_KITTOSTUB_KITCISTUB_HTTP_QEMU;
	//script = RK_JSON_KITTOSTUB_KITCISTUB_HTTP_HW;
	//script = RK_JSON_KITTOSTUB_KITCISTUB_TFTPSTUB_HTTP_QEMU;
	//script = RK_JSON_KITTOSTUB_KITCISTUB_TFTPSTUB_HTTP_HW;

	rk_alloc_run(script);
	PRINTC("\nRumpKernel Boot done.\n");

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

	PRINTC("rumpkernel cos_init\n");

	dci = cos_defcompinfo_curr_get();
	assert(dci);
	ci  = cos_compinfo_get(dci);
	assert(ci);
	currci = ci;

	cycs_per_usec = (cycles_t)cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);

	/*
	 * We don't know how many sinv's have been allocated for us,
	 * so switch back to booter to get the frontier value
	 */
	PRINTC("Fetching cap frontier from booter...");
	cos_spdid_set(cos_comp_info.cos_this_spd_id);
	spdid = cos_spdid_get();
	PRINTC("spdid: %d\n", spdid);

	ret = hypercall_comp_frontier_get(cos_spdid_get(), &vas_frontier, &cap_frontier);
	assert(!ret);
	PRINTC("done\n");

	cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ,
			BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_defcompinfo_init_ext(BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, BOOT_CAPTBL_SELF_INITTHD_CPU_BASE,
				 BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT,
				 BOOT_CAPTBL_SELF_COMP, vas_frontier, cap_frontier);

	PRINTC("Fetching boot configuration information\n");
	my_info = cos_config_info_args();
	PRINTC("Greeting key: %s\n", my_info->kvp[GREETING_KEY].key);
	PRINTC("Greeting value: %s\n", my_info->kvp[GREETING_KEY].value);

	rk_sched_init(RK_SCHED_PERIOD_US);

	PRINTC("Setting up RK\n");
	rump_booter_init((void *)0);

	/* Error, should not get here */
	assert(0);
	return;
}

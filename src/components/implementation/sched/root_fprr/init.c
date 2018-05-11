#include <sl.h>
#include <res_spec.h>
#include <hypercall.h>
#include <sched_info.h>

u32_t cycs_per_usec = 0;

#define INITIALIZE_PRIO 1
#define INITIALIZE_PERIOD_MS (4000)
#define INITIALIZE_BUDGET_MS (2000)

#define FIXED_PRIO 2
#define FIXED_PERIOD_MS (10000)
#define FIXED_BUDGET_MS (4000)

static struct sl_thd *__initializer_thd = NULL;

static int
schedinit_self(void)
{
	/* if my init is done and i've all child inits */
	if (self_init && num_child_init == sched_num_childsched_get()) return 0;

	return 1;
}

static void
__init_done(void *d)
{
	while (schedinit_self()) sl_thd_block_periodic(0);
	PRINTLOG(PRINT_DEBUG, "SELF (inc. CHILD) INIT DONE.\n");
	sl_thd_exit();

	assert(0);
}

void
sched_child_init(struct sched_childinfo *schedci)
{
	struct sl_thd *initthd = NULL;

	assert(schedci);
	initthd = sched_child_initthd_get(schedci);
	assert(initthd);
	sl_thd_param_set(initthd, sched_param_pack(SCHEDP_PRIO, FIXED_PRIO));
	sl_thd_param_set(initthd, sched_param_pack(SCHEDP_WINDOW, FIXED_PERIOD_MS));
	sl_thd_param_set(initthd, sched_param_pack(SCHEDP_BUDGET, FIXED_BUDGET_MS));

	printc("Giving child thread infinite tcap\n");
	cos_tcap_transfer(schedci->defcinfo.sched_aep.rcv, BOOT_CAPTBL_SELF_INITTCAP_BASE,
			  TCAP_RES_INF, SCHEDP_PRIO);
}

void
cos_init(void)
{
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci    = cos_compinfo_get(defci);
	int i, remaining;
	spdid_t child;
	comp_flag_t childflags;

	PRINTLOG(PRINT_DEBUG, "CPU cycles per sec: %u\n", cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE));

	cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_defcompinfo_init();

	sl_init(SL_MIN_PERIOD_US);
	sched_childinfo_init();
	__initializer_thd = sl_thd_alloc(__init_done, NULL);
	assert(__initializer_thd);
	sl_thd_param_set(__initializer_thd, sched_param_pack(SCHEDP_PRIO, INITIALIZE_PRIO));
	sl_thd_param_set(__initializer_thd, sched_param_pack(SCHEDP_WINDOW, INITIALIZE_BUDGET_MS));
	sl_thd_param_set(__initializer_thd, sched_param_pack(SCHEDP_BUDGET, INITIALIZE_PERIOD_MS));

	self_init = 1;
	hypercall_comp_init_done();

	sl_sched_loop_nonblock();

	PRINTLOG(PRINT_ERROR, "Should never have reached this point!!!\n");
	assert(0);
}


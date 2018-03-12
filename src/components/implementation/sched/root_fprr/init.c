#include <sl.h>
#include <res_spec.h>
#include <hypercall.h>
#include <sched_info.h>

u32_t cycs_per_usec = 0;
unsigned int self_init = 0;
extern int schedinit_self(void);

#define FIXED_PRIO 1
#define FIXED_PERIOD_MS (10000)
#define FIXED_BUDGET_MS (4000)

static struct sl_thd *__initializer_thd = NULL;

static void
__init_done(void *d)
{
	while (schedinit_self()) sl_thd_block_periodic(0);
	PRINTC("SELF (inc. CHILD) INIT DONE.\n");
	sl_thd_exit();

	assert(0);
}

void
cos_init(void)
{
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci    = cos_compinfo_get(defci);
	int i, remaining;
	spdid_t child;
	comp_flag_t childflags;

	PRINTC("CPU cycles per sec: %u\n", cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE));

	cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_defcompinfo_init();

	sl_init(SL_MIN_PERIOD_US);
	sched_childinfo_init();

	while ((remaining = hypercall_comp_child_next(cos_spd_id(), &child, &childflags)) >= 0) {
		struct cos_defcompinfo *child_dci = NULL;
		struct sched_childinfo *schedinfo = NULL;
		struct sl_thd          *initthd   = NULL;

		PRINTC("Initializing child component %d, is_sched=%d\n", child, childflags & COMP_FLAG_SCHED);
		schedinfo = sched_childinfo_alloc(child, 0, childflags);
		assert(schedinfo);
		child_dci = sched_child_defci_get(schedinfo);

		initthd = sl_thd_child_initaep_alloc(child_dci, childflags & COMP_FLAG_SCHED, childflags & COMP_FLAG_SCHED ? 1 : 0);
		assert(initthd);
		sched_child_initthd_set(schedinfo, initthd);

		sl_thd_param_set(initthd, sched_param_pack(SCHEDP_PRIO, FIXED_PRIO));
		sl_thd_param_set(initthd, sched_param_pack(SCHEDP_WINDOW, FIXED_PERIOD_MS));
		sl_thd_param_set(initthd, sched_param_pack(SCHEDP_BUDGET, FIXED_BUDGET_MS));

		if (!remaining) break;
	}

	assert(sched_num_child_get()); /* at least 1 child component */
	__initializer_thd = sl_thd_alloc(__init_done, NULL);
	assert(__initializer_thd);
	sl_thd_param_set(__initializer_thd, sched_param_pack(SCHEDP_PRIO, FIXED_PRIO));

	self_init = 1;
	hypercall_comp_init_done();

	sl_sched_loop_nonblock();

	PRINTC("ERROR: Should never have reached this point!!!\n");
	assert(0);
}


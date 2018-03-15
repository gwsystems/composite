#include <sl.h>
#include <res_spec.h>
#include <hypercall.h>
#include <sched_info.h>

#define FIXED_PRIO 1

u32_t cycs_per_usec = 0;

void
sched_child_init(struct sched_childinfo *schedci)
{
	struct sl_thd *initthd = NULL;

	assert(schedci);

	initthd = sched_child_initthd_get(schedci);
	assert(initthd);
	sl_thd_param_set(initthd, sched_param_pack(SCHEDP_PRIO, FIXED_PRIO));
}

void
cos_init(void)
{
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci    = cos_compinfo_get(defci);

	PRINTC("CPU cycles per sec: %u\n", cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE));

	cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_defcompinfo_init();

	sl_init(SL_MIN_PERIOD_US);
	sched_childinfo_init_raw();
	hypercall_comp_init_done();

	sl_sched_loop_nonblock();

	PRINTC("ERROR: Should never have reached this point!!!\n");
	assert(0);
}

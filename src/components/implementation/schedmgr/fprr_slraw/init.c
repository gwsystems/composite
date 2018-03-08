#include <sl.h>
#include <res_spec.h>
#include <hypercall.h>
#include "sched_info.h"

#define FIXED_PRIO 1

u32_t cycs_per_usec = 0;

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
		struct cos_defcompinfo  *child_dci = NULL;
		struct sched_childinfo *schedinfo = NULL;
		struct sl_thd           *initthd   = NULL;
		compcap_t comp_cap;

		PRINTC("Initializing child component %u, is_sched=%d\n", child, childflags & COMP_FLAG_SCHED);
		comp_cap = hypercall_comp_compcap_get(child);
		schedinfo = sched_childinfo_alloc(child, comp_cap, childflags);
		assert(schedinfo);
		child_dci = sched_child_defci_get(schedinfo);

		initthd = sl_thd_child_initaep_alloc(child_dci, childflags & COMP_FLAG_SCHED, childflags & COMP_FLAG_SCHED ? 1 : 0);
		assert(initthd);
		sched_child_initthd_set(schedinfo, initthd);

		sl_thd_param_set(initthd, sched_param_pack(SCHEDP_PRIO, FIXED_PRIO));
		if (!remaining) break;
	}

	assert(sched_num_child_get()); /* at least 1 child component */
	hypercall_comp_init_done();

	sl_sched_loop_nonblock();

	PRINTC("ERROR: Should never have reached this point!!!\n");
	assert(0);
}

/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#include <sl.h>
#include <res_spec.h>
#include <hypercall.h>
#include <sl_child.h>
#include <rk.h>

u32_t cycs_per_usec = 0;
extern cbuf_t parent_schedinit_child(void);

#define FIXED_PRIO 2
#define FIXED_BUDGET_MS 2000
#define FIXED_PERIOD_MS 10000

extern void rk_dummy_init(void);
extern void rk_dummy_thdinit(thdid_t, int);

static int
schedinit_self(void)
{
	parent_schedinit_child();

	return 0;
}

void
sched_child_init(struct sl_thd *initthd, int isaep)
{
	assert(initthd);
	sl_thd_param_set(initthd, sched_param_pack(SCHEDP_PRIO, FIXED_PRIO));
}

static void
rk_child_initthd_create(void)
{
	int remaining = 0;
	spdid_t child;
	comp_flag_t childflags;
	int num_child = 0;

	while ((remaining = hypercall_comp_child_next(cos_spd_id(), &child, &childflags)) >= 0) {
		struct cos_defcompinfo child_dci;
		struct sl_thd *t = NULL;

		assert(child);
		assert(!(childflags & COMP_FLAG_SCHED));
		num_child ++;
		PRINTC("CHILD:%d thread created from RK DUMMY COMPONENT\n", child);
		cos_defcompinfo_childid_init(&child_dci, child);

		t = sl_thd_initaep_alloc(&child_dci, 0, 0, 0, 0, 0, 0);
		assert(t);
		sched_child_init(t, 0);

		if (!remaining) break;
	}
	PRINTC("Child components: %d\n", num_child);

	assert(num_child);
}

void
cos_init(void)
{
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci    = cos_compinfo_get(defci);
	static unsigned long first = NUM_CPU + 1, init_done[NUM_CPU] = { 0 };
	static u32_t cpubmp[NUM_CPU_BMP_WORDS] = { 0 };
	int i, only_one = 1;

	PRINTLOG(PRINT_DEBUG, "CPU cycles per sec: %u\n", cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE));

	if (ps_cas(&first, NUM_CPU + 1, cos_cpuid())) {
		cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
		cos_defcompinfo_init();
		cos_init_args_cpubmp(cpubmp);

		rk_dummy_init();
	} else {
		while (!ps_load(&init_done[first])) ;

		cos_defcompinfo_sched_init();
	}
	ps_faa(&init_done[cos_cpuid()], 1);

	PRINTC("RK DUMMY COMPONENT BOOTING UP!\n");
	/* make sure the INITTHD of the scheduler is created on all cores.. for cross-core sl initialization to work! */
	for (i = 0; i < NUM_CPU; i++) {
		if (!bitmap_check(cpubmp, i)) continue;

		/* only run on one core! */
		assert(only_one);
		only_one = 0;
		while (!ps_load(&init_done[i])) ;
	}

	sl_init_cpubmp(SL_MIN_PERIOD_US, cpubmp);
	rk_child_initthd_create();
	schedinit_self();
	PRINTC("RK DUMMY COMPONENT UP AND RUNNING!\n");

	sl_sched_loop();

	PRINTLOG(PRINT_ERROR, "Should never have reached this point!!!\n");
	assert(0);
}

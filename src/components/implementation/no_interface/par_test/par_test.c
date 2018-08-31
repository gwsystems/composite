/*
 * Copyright 2018, Chris Gill (WUSTL) cdgill@cse.wustl.edu, 
 * Phani Gadepalli and Gabriel Parmer (GWU) gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <llprint.h>
#include <res_spec.h>
#include <hypercall.h>
#include <sched.h>
#include <cos_time.h>
#include <workgen.h>

#define LOW_WORK_us 2000
#define HIGH_WORK_us 1000

#define SL_FPRR_NPRIOS 32

#define LOWEST_PRIORITY (SL_FPRR_NPRIOS - 1)

#define LOW_PRIORITY (LOWEST_PRIORITY - 1)
#define HIGH_PRIORITY (LOWEST_PRIORITY - 10)

static int lowest_was_scheduled[NUM_CPU];
static u32_t cycs_per_usec = 0;

static void
thd1_fn()
{
	int i;
	for (i=1; i<=10; ++i) {
	  printc("[l %llu]", workgen_usecs (LOW_WORK_us));
        }

	sched_thd_exit();
}

static void
thd2_fn()
{
	int i;
	for (i=1; i<=10; ++i) {
	  printc("[h %llu]", workgen_usecs (HIGH_WORK_us));
        }

	sched_thd_exit();
}

static void
allocator_thread_fn()
{
	thdid_t tid1, tid2;
	cycles_t wakeup;

	PRINTLOG(PRINT_DEBUG, "Unit-test in llOS allocator_thread_fn\n");

	tid1 = sched_thd_create(thd1_fn, NULL);
	sched_thd_param_set(tid1, sched_param_pack(SCHEDP_PRIO, LOW_PRIORITY));

	tid2 = sched_thd_create(thd2_fn, NULL);
	sched_thd_param_set(tid2, sched_param_pack(SCHEDP_PRIO, HIGH_PRIORITY));

	wakeup = time_now() + time_usec2cyc(1000 * 1000);
	sched_thd_block_timeout(0, wakeup);


        /* Don't delete unless threads run forever */
        /*
	sched_thd_delete(tid1);
	sched_thd_delete(tid2);
	*/

	sched_thd_exit();
}

void
cos_init(void)
{
	spdid_t child;
	comp_flag_t childflag;

	cycs_per_usec = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);

	PRINTLOG(PRINT_DEBUG, "Unit-test new llOS parallelism manager component\n");
	assert(hypercall_comp_child_next(cos_spd_id(), &child, &childflag) == -1);

        allocator_thread_fn();

        sched_thd_exit();

	/* should never get here */
	PRINTLOG(PRINT_ERROR, "Cannot reach here in par_test.c!\n");
	assert(0);
}

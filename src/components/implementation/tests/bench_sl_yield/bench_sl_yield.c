/*
 * Copyright 2016, Phani Gadepalli, Runyu Pan and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <cos_defkernel_api.h>
#include <llprint.h>
#include <res_spec.h>
#include <sl.h>
#include <perfdata.h>

#undef YIELD_TRACE_DEBUG
#ifdef YIELD_TRACE_DEBUG
#define debug(format, ...) printc(format, ##__VA_ARGS__)
#else
#define debug(format, ...)
#endif

/* lo and hi is actually running at the same prio */
#define ITERATION 10000
/* #define PRINT_ALL */

/* Ensure this is the same as what is in sl_mod_fprr.c */
#define SL_FPRR_NPRIOS 32

#define PRIORITY (SL_FPRR_NPRIOS - 5)
#define HIGH_PRIORITY (SL_FPRR_NPRIOS - 6)
#define LOW_PRIORITY (SL_FPRR_NPRIOS - 4)

struct sl_thd *testing_thread;
thdid_t        thdid1, thdid2;

volatile cycles_t start;
volatile cycles_t end;

struct perfdata perf;
cycles_t        result[ITERATION] = {
  0,
};

static void
thd1_fn()
{
	/* Never stops running; low priority controls how many iters to run. */
	while (1) {
		debug("h1,");
		sl_thd_yield(thdid2);
		debug("h2,");
	}
}

static void
thd2_fn()
{
	int i;
	int first = 0;

	for (i = 0; i < ITERATION + 1; i++) {
		debug("l1,");

		start = time_now();
		sl_thd_yield(thdid1);
		end = time_now();

		debug("l2,");

		if (first == 0)
			first = 1;
		else
			perfdata_add(&perf, end - start);
	}

	perfdata_calc(&perf);
#ifdef PRINT_ALL
	perfdata_all(&perf);
#else
	perfdata_print(&perf);
#endif

	while (1)
		;
}

static void
run_tests()
{
	struct sl_thd *thd1, *thd2;

	thd1 = sl_thd_alloc(thd1_fn, NULL);
	sl_thd_param_set(thd1, sched_param_pack(SCHEDP_PRIO, PRIORITY));
	thdid1 = sl_thd_thdid(thd1);

	thd2 = sl_thd_alloc(thd2_fn, NULL);
	sl_thd_param_set(thd2, sched_param_pack(SCHEDP_PRIO, PRIORITY));
	thdid2 = sl_thd_thdid(thd2);

	PRINTC("Thread creation done!\n");
	perfdata_init(&perf, "Context switch time", result, ITERATION);

	sl_thd_param_set(testing_thread, sched_param_pack(SCHEDP_PRIO, LOW_PRIORITY));
	PRINTC("Lowering priority...\n");
	while (1)
		;
}

void
cos_init(void)
{
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo *   ci    = cos_compinfo_get(defci);

	PRINTC("Thread switch benchmark for the scheduling library (sl)\n");
	cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_defcompinfo_init();

	sl_init(SL_MIN_PERIOD_US);

	testing_thread = sl_thd_alloc(run_tests, NULL);
	sl_thd_param_set(testing_thread, sched_param_pack(SCHEDP_PRIO, HIGH_PRIORITY));

	PRINTC("Now running sl\n");

	sl_sched_loop();

	assert(0);

	return;
}

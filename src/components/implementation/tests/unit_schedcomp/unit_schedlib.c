/*
 * Copyright 2016, Phani Gadepalli and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <llprint.h>
#include <sl.h>
#include <hypercall.h>

/* sl also defines a SPIN macro */
#undef SPIN
#define SPIN(iters)                                \
	do {                                       \
		if (iters > 0) {                   \
			for (; iters > 0; iters--) \
				;                  \
		} else {                           \
			while (1)                  \
				;                  \
		}                                  \
	} while (0)


#define N_TESTTHDS 8
#define WORKITERS 10000

#define N_TESTTHDS_PERF 2
#define PERF_ITERS 1000000

static volatile cycles_t mid_cycs = 0;
static volatile int testing = 1;

void
test_thd_perffn(void *data)
{
	cycles_t start_cycs = 0, end_cycs = 0, wc_cycs = 0, total_cycs = 0;
	unsigned int i = 0;

	rdtscll(start_cycs);
	sl_thd_yield(0);
	rdtscll(end_cycs);
	assert(mid_cycs && mid_cycs > start_cycs && mid_cycs < end_cycs);

	for (i = 0; i < PERF_ITERS; i++) {
		cycles_t diff1_cycs = 0, diff2_cycs = 0;

		mid_cycs = 0;
		rdtscll(start_cycs);
		sl_thd_yield(0);
		rdtscll(end_cycs);
		assert(mid_cycs && mid_cycs > start_cycs && mid_cycs < end_cycs);

		diff1_cycs = mid_cycs - start_cycs;
		diff2_cycs = end_cycs - mid_cycs;

		if (diff1_cycs > wc_cycs) wc_cycs = diff1_cycs;
		if (diff2_cycs > wc_cycs) wc_cycs = diff2_cycs;
		total_cycs += (diff1_cycs + diff2_cycs);
	}

	PRINTC("SWITCH UBENCH: avg: %llu, wc: %llu, iters:%u\n", (total_cycs / (2 * PERF_ITERS)), wc_cycs, PERF_ITERS);
	testing = 0;
	/* done testing! let the spinfn cleanup! */
	sl_thd_yield(0);

	sl_thd_exit();
}

void
test_thd_spinfn(void *data)
{
	while (likely(testing)) {
		rdtscll(mid_cycs);
		sl_thd_yield(0);
	}

	sl_thd_exit();
}

void
test_thd_fn(void *data)
{
	while (1) {
		int workiters = WORKITERS * ((int)data);

		printc("%c", 'a' + (int)data);
		//SPIN(workiters);
		sl_thd_yield(0);
	}
}

void
test_yield_perf(void)
{
	int                     i;
	struct sl_thd          *threads[N_TESTTHDS_PERF];
	union sched_param_union sp = {.c = {.type = SCHEDP_PRIO, .value = 31}};

	for (i = 0; i < N_TESTTHDS_PERF; i++) {
		if (i == 1) threads[i] = sl_thd_alloc(test_thd_perffn, (void *)&threads[0]);
		else        threads[i] = sl_thd_alloc(test_thd_spinfn, NULL);
		assert(threads[i]);
		sl_thd_param_set(threads[i], sp.v);
		PRINTC("Thread %u:%lu created\n", sl_thd_thdid(threads[i]), sl_thd_thdcap(threads[i]));
	}
}

void
test_yields(void)
{
	int                     i;
	struct sl_thd *         threads[N_TESTTHDS];
	union sched_param_union sp = {.c = {.type = SCHEDP_PRIO, .value = 10}};

	for (i = 0; i < N_TESTTHDS; i++) {
		threads[i] = sl_thd_alloc(test_thd_fn, (void *)i);
		assert(threads[i]);
		sl_thd_param_set(threads[i], sp.v);
		PRINTC("Thread %u:%lu created\n", sl_thd_thdid(threads[i]), sl_thd_thdcap(threads[i]));
	}
}

void
cos_init(void)
{
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo *   ci    = cos_compinfo_get(defci);
	static int first_time = 1, init_done = 0;

	PRINTC("Unit-test for the scheduling library (sl) with capmgr usage\n");
	PRINTC("CPU cycles per sec: %u\n", cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE));

	if (first_time) {
		first_time = 0;
		cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
		cos_defcompinfo_init();
		init_done = 1;
	} else {
		while (!init_done) ;

		cos_defcompinfo_sched_init();
	}

	sl_init(SL_MIN_PERIOD_US);

	//test_yield_perf();
	test_yields();
	hypercall_comp_init_done();

	sl_sched_loop_nonblock();

	assert(0);

	return;
}

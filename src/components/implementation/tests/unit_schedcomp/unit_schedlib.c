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
#include <cos_rdtsc.h>

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

#define PERF_ITERS 1000000

static cycles_t rdtscp_min = 0, rdtscp_max = 0, rdtscp_avg = 0;
static volatile int switched = 0;
static volatile cycles_t mid_cycs = 0;
static volatile int testing = 1;
static struct sl_thd *perf_thd, *spin_thd;

void
test_thd_perffn(void *data)
{
	thdid_t yield_to = sl_thd_thdid(spin_thd);
	cycles_t start_cycs = 0, end_cycs = 0, wc_cycs = 0, total_cycs = 0, bc_cycs = 500;
	unsigned int i = 0;
	int ret = 0;

	assert(perf_thd == sl_thd_curr());
	rdtscll(start_cycs);
	//printc("a");
	sl_thd_yield(yield_to);
	//ret = sl_thd_dispatch(spin_thd, cos_sched_sync(), perf_thd);
	//sl_thd_yield_thd_c(perf_thd, spin_thd);
	//sl_thd_yield_thd(spin_thd);
	//assert(ret == 0);
	rdtscll(end_cycs);
	//assert(mid_cycs && mid_cycs > start_cycs && mid_cycs < end_cycs);
	assert(switched);

	for (i = 0; i < PERF_ITERS; i++) {
		cycles_t diff1_cycs = 0, diff2_cycs = 0;

		end_cycs = start_cycs = 0;
		//mid_cycs = 0;
		switched = 0;
		cos_rdtscp(start_cycs);
		//rdtscll(start_cycs);
		//ret = sl_thd_dispatch(spin_thd, cos_sched_sync(), perf_thd);
		//printc("a");
		sl_thd_yield(yield_to);
		//sl_thd_yield_thd_c(perf_thd, spin_thd);
		//sl_thd_yield_thd(spin_thd);
		//rdtscll(end_cycs);
		cos_rdtscp(end_cycs);
		assert(switched);
		assert(ret == 0);
		//assert(mid_cycs && mid_cycs > start_cycs && mid_cycs < end_cycs);

		//diff1_cycs = mid_cycs - start_cycs;
		diff2_cycs = end_cycs - start_cycs;
		assert(diff2_cycs > rdtscp_min);
		diff2_cycs -= rdtscp_min;

		//if (diff1_cycs > wc_cycs) wc_cycs = diff1_cycs;
		if (diff2_cycs > wc_cycs) wc_cycs = diff2_cycs;
		if (diff2_cycs < bc_cycs) bc_cycs = diff2_cycs;
		total_cycs += diff2_cycs;
	}

	PRINTC("SWITCH UBENCH (2 switches): avg: %llu, wc: %llu, bc: %llu, iters:%u\n", (total_cycs / (PERF_ITERS)), wc_cycs, bc_cycs, PERF_ITERS);
	testing = 0;
	/* done testing! free the spin thread! */
	while (1) ;
//	sl_thd_free(spin_thd);

//	sl_thd_exit();
}

void
test_thd_spinfn(void *data)
{
	thdid_t yield_to = sl_thd_thdid(perf_thd);
	assert(sl_thd_curr() == spin_thd);

	while (likely(testing)) {
		//rdtscll(mid_cycs);
		switched = 1;
		//sl_thd_dispatch(perf_thd, cos_sched_sync(), spin_thd);
		//printc("b");
		sl_thd_yield(yield_to);
		//sl_thd_yield_thd_c(spin_thd, perf_thd);
		//sl_thd_yield_thd(perf_thd);
	}

	//sl_thd_dispatch(perf_thd, cos_sched_sync(), spin_thd);
	sl_thd_yield(yield_to);
	//sl_thd_yield_thd_c(spin_thd, perf_thd);
	//sl_thd_yield_thd(perf_thd);
	//assert(0);
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
	union sched_param_union sp = {.c = {.type = SCHEDP_PRIO, .value = 31}};

	assert(NUM_CPU == 1);

	spin_thd = sl_thd_alloc(test_thd_spinfn, NULL);
	assert(spin_thd);
	sl_thd_param_set(spin_thd, sp.v);
	PRINTC("Spin thread %u:%lu created\n", sl_thd_thdid(spin_thd), sl_thd_thdcap(spin_thd));

	perf_thd = sl_thd_alloc(test_thd_perffn, NULL);
	assert(perf_thd);
	sl_thd_param_set(perf_thd, sp.v);
	PRINTC("Perf thread %u:%lu created\n", sl_thd_thdid(perf_thd), sl_thd_thdcap(perf_thd));

	sl_thd_yield(sl_thd_thdid(perf_thd));
	//sl_thd_dispatch(perf_thd, cos_sched_sync(), sl_thd_curr());
	//sl_thd_yield_thd_c(sl_thd_curr(), perf_thd);
	//sl_thd_yield_thd(perf_thd);
	while (1);
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
		cos_rdtscp_calib(&rdtscp_min, &rdtscp_avg, &rdtscp_max);
		PRINTC("RDTSCP MIN:%llu MAX:%llu AVG:%llu\n", rdtscp_min, rdtscp_max, rdtscp_avg);

		init_done = 1;
	} else {
		while (!init_done) ;

		cos_defcompinfo_sched_init();
	}

	sl_init(SL_MIN_PERIOD_US);
	hypercall_comp_init_done();

	test_yield_perf();
	//test_yields();

	sl_sched_loop_nonblock();

	assert(0);

	return;
}

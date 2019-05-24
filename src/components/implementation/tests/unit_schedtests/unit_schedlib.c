/*
 * Copyright 2016, Phani Gadepalli and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <cos_component.h>
#include <cobj_format.h>
#include <cos_defkernel_api.h>
#include <llprint.h>
#include <sl.h>
#include <cos_dcb.h>

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

#define MAGIC_RET 0xDEADBEEF

#define INV_TEST
static volatile cycles_t mid_cycs = 0;
static volatile int testing = 1;


int
test_serverfn(int a, int b, int c)
{
        //rdtscll(midinv_cycles[cos_cpuid()]);
        return MAGIC_RET;
}

extern void *__inv_test_serverfn(int a, int b, int c);

static inline int
call_cap_mb(u32_t cap_no, int arg1, int arg2, int arg3)
{
        int ret;

        /*
         * Which stack should we use for this invocation?  Simple, use
         * this stack, at the current sp.  This is essentially a
         * function call into another component, with odd calling
         * conventions.
         */
        cap_no = (cap_no + 1) << COS_CAPABILITY_OFFSET;

        __asm__ __volatile__("pushl %%ebp\n\t"
                             "movl %%esp, %%ebp\n\t"
                             "movl %%esp, %%edx\n\t"
                             "movl $1f, %%ecx\n\t"
                             "sysenter\n\t"
                             "1:\n\t"
                             "popl %%ebp"
                             : "=a"(ret)
                             : "a"(cap_no), "b"(arg1), "S"(arg2), "D"(arg3)
                             : "memory", "cc", "ecx", "edx");

        return ret;
}

sinvcap_t sinv_cap = 0;

static inline void
test_inv_setup(void)
{
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo *   ci    = cos_compinfo_get(defci);
        compcap_t    cc;
        sinvcap_t    ic;
        int          i;
        unsigned int ret;

        cc = cos_comp_alloc(ci, ci->captbl_cap, ci->pgtbl_cap, 0, (vaddr_t)NULL, 0);
        assert(cc > 0);
        ic = cos_sinv_alloc(ci, cc, (vaddr_t)__inv_test_serverfn, 0);
        assert(ic > 0);
        ret = call_cap_mb(ic, 1, 2, 3);
        assert(ret == (int)MAGIC_RET);

	sinv_cap = ic;
}

static struct sl_thd *perf_thd = NULL, *spin_thd = NULL;

void
test_thd_perffn(void *data)
{
	cycles_t start_cycs = 0, end_cycs = 0, wc_cycs = 0, total_cycs = 0;
	unsigned int i = 0;
	struct sl_thd *c = sl_thd_curr();

	rdtscll(start_cycs);
	sl_thd_yield_thd(spin_thd);
	rdtscll(end_cycs);
	assert(mid_cycs && mid_cycs > start_cycs && mid_cycs < end_cycs);

	for (i = 0; i < PERF_ITERS; i++) {
		cycles_t diff1_cycs = 0, diff2_cycs = 0;

		mid_cycs = 0;
		rdtscll(start_cycs);
		sl_thd_yield_thd_c(c, spin_thd);
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
	sl_thd_yield_thd(spin_thd);

	sl_thd_exit();
}

void
test_inv_perffn(void *data)
{
	cycles_t start_cycs = 0, end_cycs = 0, wc_cycs = 0, total_cycs = 0;
	unsigned int i = 0;
	struct sl_thd *c = sl_thd_curr();

	test_inv_setup();

	rdtscll(start_cycs);
	sl_thd_yield_thd(spin_thd);
	rdtscll(end_cycs);
	assert(mid_cycs && mid_cycs > start_cycs && mid_cycs < end_cycs);

	for (i = 0; i < PERF_ITERS; i++) {
		cycles_t diff_cycs = 0;
		int ret;

		sl_thd_yield_thd_c(c, spin_thd);
		mid_cycs = 0;
		rdtscll(start_cycs);
		ret = call_cap_mb(sinv_cap, 1, 2, 3);
		rdtscll(end_cycs);
		assert(ret == (int)MAGIC_RET);
//		assert(mid_cycs && mid_cycs > start_cycs && mid_cycs < end_cycs);
//
//		diff1_cycs = mid_cycs - start_cycs;
//		diff2_cycs = end_cycs - mid_cycs;
//
//		if (diff1_cycs > wc_cycs) wc_cycs = diff1_cycs;
//		if (diff2_cycs > wc_cycs) wc_cycs = diff2_cycs;
//		total_cycs += (diff1_cycs + diff2_cycs);
		diff_cycs = end_cycs - start_cycs;
		if (diff_cycs > wc_cycs) wc_cycs = diff_cycs;
		total_cycs += diff_cycs;
	}

	PRINTC("INV UBENCH: avg: %llu, wc: %llu, iters:%u\n", (total_cycs / PERF_ITERS), wc_cycs, PERF_ITERS);
	testing = 0;
	/* done testing! let the spinfn cleanup! */
	sl_thd_yield_thd(spin_thd);

	sl_thd_exit();
}

void
test_thd_spinfn(void *data)
{
	struct sl_thd *c = sl_thd_curr();

	while (likely(testing)) {
		rdtscll(mid_cycs);
		sl_thd_yield_thd_c(c, perf_thd);
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
		if (i == 1) {
#ifdef INV_TEST
			threads[i] = sl_thd_alloc(test_inv_perffn, (void *)&threads[0]);
#else
			threads[i] = sl_thd_alloc(test_thd_perffn, (void *)&threads[0]);
#endif
			perf_thd = threads[i];
		} else {
			threads[i] = sl_thd_alloc(test_thd_spinfn, NULL);
			spin_thd = threads[i];
		}
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
test_high(void *data)
{
	struct sl_thd *t = data;

	while (1) {
		sl_thd_yield(sl_thd_thdid(t));
		printc("h");
	}
}

void
test_low(void *data)
{
	while (1) {
		int workiters = WORKITERS * 10;
		SPIN(workiters);
		printc("l");
	}
}

void
test_blocking_directed_yield(void)
{
	struct sl_thd *         low, *high;
	union sched_param_union sph = {.c = {.type = SCHEDP_PRIO, .value = 5}};
	union sched_param_union spl = {.c = {.type = SCHEDP_PRIO, .value = 10}};

	low  = sl_thd_alloc(test_low, NULL);
	high = sl_thd_alloc(test_high, low);
	sl_thd_param_set(low, spl.v);
	sl_thd_param_set(high, sph.v);
}

#define TEST_ITERS 1000

void
test_high_wakeup(void *data)
{
	unsigned int   toggle = 0, iters = 0;
	struct sl_thd *t     = data;
	cycles_t       start = sl_now();

	while (1) {
		cycles_t timeout = sl_now() + sl_usec2cyc(100);

		if (toggle % 10 == 0)
			printc(".h:%llums.", sl_cyc2usec(sl_thd_block_timeout(0, timeout)));
		else
			printc(".h:%up.", sl_thd_block_periodic(0));

		toggle++;
		iters++;

		if (iters == TEST_ITERS) {
			printc("\nTest done! (Duration: %llu ms)\n", sl_cyc2usec(sl_now() - start) / 1000);
			printc("Deleting all threads. Idle thread should take over!\n");
			sl_thd_free(t);
			sl_thd_free(sl_thd_curr());

			/* should not be scheduled. */
			assert(0);
		}
	}
}

void
test_timeout_wakeup(void)
{
	struct sl_thd *         low, *high;
	union sched_param_union sph = {.c = {.type = SCHEDP_PRIO, .value = 5}};
	union sched_param_union spl = {.c = {.type = SCHEDP_PRIO, .value = 10}};
	union sched_param_union spw = {.c = {.type = SCHEDP_WINDOW, .value = 1000}};

	low = sl_thd_alloc(test_low, NULL);
	sl_thd_param_set(low, spl.v);
	sl_thd_param_set(low, spw.v);

	high = sl_thd_alloc(test_high_wakeup, low);
	sl_thd_param_set(high, sph.v);
	sl_thd_param_set(high, spw.v);
}

void
cos_init(void)
{
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo *   ci    = cos_compinfo_get(defci);

	printc("Unit-test for the scheduling library (sl)\n");
	cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_defcompinfo_llinit();
	cos_dcb_info_init_curr();
	sl_init(SL_MIN_PERIOD_US*100);

	test_yield_perf();
	//test_yields();
	//test_blocking_directed_yield();
	//test_timeout_wakeup();

	sl_sched_loop_nonblock();

	assert(0);

	return;
}

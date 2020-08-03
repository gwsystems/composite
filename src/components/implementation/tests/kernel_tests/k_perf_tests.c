#include <stdint.h>
#include "kernel_tests.h"

static struct perfdata pd[NUM_CPU] CACHE_ALIGNED;
extern struct results  result_test_timer;
extern struct results  result_budgets_single;
extern struct results  result_sinv;
struct results  result_switch, result_thd_switch;
struct results  result_async_roundtrip, result_async_oneway;

#define ARRAY_SIZE 10000
static cycles_t test_results[ARRAY_SIZE] = { 0 };

unsigned int cyc_per_usec;
static volatile arcvcap_t rcc_global[NUM_CPU], rcp_global[NUM_CPU];
static volatile asndcap_t scc_global[NUM_CPU], scp_global[NUM_CPU];
static int                async_test_flag_[NUM_CPU] = { 0 };
volatile cycles_t         main_thd = 0, side_thd = 0;

/*
 *  Measuremet of COS_SWITCH and COS_THD_SWITCH
 *  Roundtrip measurement of 2 thread that switch back and forth
 */

static void
bounceback(void *d)
{
        while (1) {
                rdtscll(side_thd);
                cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);
        }
}

static void
test_thds_create_switch(void)
{
        thdcap_t ts;
        int      ret, i;

        perfdata_init(&pd[cos_cpuid()], "COS THD => COS_THD_SWITCH", test_results, ARRAY_SIZE);

        ts = cos_thd_alloc(&booter_info, booter_info.comp_cap, bounceback, NULL);
        if (EXPECT_LL_LT(1, ts, "Thread Creation: Cannot Allocate")) {
                return;
        }

        for (i = 0; i < ITER; i++) {
                rdtscll(main_thd);
                ret = cos_thd_switch(ts);
                EXPECT_LL_NEQ(0, ret, "COS Switch Error");

                perfdata_add(&pd[cos_cpuid()], (side_thd - main_thd));
        }

        perfdata_calc(&pd[cos_cpuid()]);
	results_save(&result_thd_switch, &pd[cos_cpuid()]);

        perfdata_init(&pd[cos_cpuid()], "COS THD => COS_SWITCH", test_results, ARRAY_SIZE);

        for (i = 0; i < ITER; i++) {
                rdtscll(main_thd);
                ret = cos_switch(ts, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, 0, 0, 0, 0);
                EXPECT_LL_NEQ(0, ret, "COS Switch Error");

                perfdata_add(&pd[cos_cpuid()], (side_thd - main_thd));
        }

        perfdata_calc(&pd[cos_cpuid()]);
	results_save(&result_switch, &pd[cos_cpuid()]);
}

/*
 * Asychronous RCV and SND:
 *   * Roundtrip: 2 Thd that bounce between eachother through cos_rcv() and cos_asnd()
 *   * One way: 1 thd send and 1 thd receives. When the receivers block the sender will be enqueue
 */
static cycles_t end_oneway = 0;

static void
async_thd_fn_perf(void *thdcap)
{
        thdcap_t  tc = (thdcap_t)thdcap;
        asndcap_t sc = scc_global[cos_cpuid()];
        arcvcap_t rc = rcc_global[cos_cpuid()];
        int           i, ret, pending = 0;

        for (i = 0; i < ITER; i++) {
                cos_rcv(rc, 0, NULL);
                cos_asnd(sc, 1);
        }

        for (i = 0; i < ITER + 1; i++) {
                cos_rcv(rc, 0, NULL);
		rdtscll(end_oneway);
        }

        EXPECT_LL_NEQ(1, 0, "Error, shouldn't get here!\n");
        assert(0);
}

static void
async_thd_parent_perf(void *thdcap)
{
        thdcap_t  tc = (thdcap_t)thdcap;
        asndcap_t sc = scp_global[cos_cpuid()];
        arcvcap_t rc = rcc_global[cos_cpuid()];
        long long e = 0, s = 0;
        int           i, pending = 0;

        perfdata_init(&pd[cos_cpuid()], "Async Endpoints => Roundtrip", test_results, ARRAY_SIZE);

        for (i = 0; i < ITER; i++) {
                rdtscll(s);
                cos_asnd(sc, 1);
                cos_rcv(rc, 0, NULL);
                rdtscll(e);

                perfdata_add(&pd[cos_cpuid()], (e - s));
        }

        perfdata_calc(&pd[cos_cpuid()]);
	results_save(&result_async_roundtrip, &pd[cos_cpuid()]);

        perfdata_init(&pd[cos_cpuid()], "Async Endpoints => One Way", test_results, ARRAY_SIZE);

	i = 0;
	while (i < ITER) {
		end_oneway = 0;
                rdtscll(s);
                cos_asnd(sc, 1);
		e = end_oneway;
		if (unlikely(e == 0)) continue;
		i++;
                perfdata_add(&pd[cos_cpuid()], (e - s));
        }

        perfdata_calc(&pd[cos_cpuid()]);
	results_save(&result_async_oneway, &pd[cos_cpuid()]);

        async_test_flag_[cos_cpuid()] = 0;
        while (1) cos_thd_switch(tc);
}

static void
test_async_endpoints_perf(void)
{
        thdcap_t  tcp, tcc;
        tcap_t    tccp, tccc;
        arcvcap_t rcp, rcc;

        /* parent rcv capabilities */
        tcp = cos_thd_alloc(&booter_info, booter_info.comp_cap, async_thd_parent_perf,
                                                (void *)BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);
        if(EXPECT_LL_LT(1, tcp, "Test Async Endpoints")) return;
        tccp = cos_tcap_alloc(&booter_info);
        if(EXPECT_LL_LT(1, tccp, "Test Async Endpoints")) return;
        rcp = cos_arcv_alloc(&booter_info, tcp, tccp, booter_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE);
        if(EXPECT_LL_LT(1, rcp, "Test Async Endpoints")) return;
        if(EXPECT_LL_NEQ(0,cos_tcap_transfer(rcp, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF,
                                                                                 TCAP_PRIO_MAX + 1), "Test Async Endpoints")) {
                return;
        }

        /* child rcv capabilities */
        tcc = cos_thd_alloc(&booter_info, booter_info.comp_cap, async_thd_fn_perf, (void *)tcp);
        if(EXPECT_LL_LT(1, tcc, "Test Async Endpoints")) return;
        tccc = cos_tcap_alloc(&booter_info);
        if(EXPECT_LL_LT(1, tccc, "Test Async Endpoints")) return;
        rcc = cos_arcv_alloc(&booter_info, tcc, tccc, booter_info.comp_cap, rcp);
        if(EXPECT_LL_LT(1, rcc, "Test Async Endpoints")) return;
        if(EXPECT_LL_NEQ(0,cos_tcap_transfer(rcc, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF,
                                                                                 TCAP_PRIO_MAX), "Test Async Endpoints"))
                 return;

        /* make the snd channel to the child */
        scp_global[cos_cpuid()] = cos_asnd_alloc(&booter_info, rcc, booter_info.captbl_cap);
        if(EXPECT_LL_EQ(0, scp_global[cos_cpuid()], "Test Async Endpoints")) return;

        /* make the snd channel to the parent */
        scc_global[cos_cpuid()] = cos_asnd_alloc(&booter_info, rcp, booter_info.captbl_cap);
        if(EXPECT_LL_EQ(0, scp_global[cos_cpuid()], "Test Async Endpoints")) return;

        rcc_global[cos_cpuid()] = rcc;
        rcp_global[cos_cpuid()] = rcp;

        async_test_flag_[cos_cpuid()] = 1;
        while (async_test_flag_[cos_cpuid()]) cos_thd_switch(tcp);
}

void
test_print_ubench(void)
{
	results_split_print(&result_thd_switch, "Threads => cos_thd_switch:");
	results_split_print(&result_switch, "Threads => cos_switch:");
	results_split_print(&result_async_roundtrip, "Async => Roundtrip:");
	results_split_print(&result_async_oneway, "Async => Oneway:");
	results_split_print(&result_sinv, "Synchronous Invocations:");
	results_split_print(&result_test_timer, "Timer => Timeout Overhead:");
	results_split_print(&result_budgets_single, "Timer => Budget Based:");
}

void
test_run_perf_kernel(void)
{
	printc("\n");
	PRINTC("uBenchamarks Started:\n\n");
        cyc_per_usec = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);
        test_thds_create_switch();
        test_async_endpoints_perf();
        test_print_ubench();
}

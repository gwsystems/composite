#include <stdint.h>

#include "micro_booter.h"

static struct perfdata pd[NUM_CPU] CACHE_ALIGNED;
extern struct perfdata result_test_timer;
extern struct perfdata result_budgets_single;
extern struct perfdata result_sinv;

unsigned int cyc_per_usec;
static volatile arcvcap_t rcc_global[NUM_CPU], rcp_global[NUM_CPU];
static volatile asndcap_t scc_global[NUM_CPU], scp_global[NUM_CPU];
static int                async_test_flag_[NUM_CPU] = { 0 };
volatile cycles_t         main_thd = 0, side_thd = 0;

static void
spinner(void *d)
{
    while (1) {
        rdtscll(side_thd); 
        cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);
    }
}

static void
test_thds_create_switch(void)
{
    thdcap_t            ts;
    int                 ret, i;

    perfdata_init(&pd[cos_cpuid()], "COS THD => COS_THD_SWITCH");

    ts = cos_thd_alloc(&booter_info, booter_info.comp_cap, spinner, NULL);
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

    PRINTC("\tCOS THD => COS_THD_SWITCH:\t\tAVG:%llu, MAX:%llu, MIN:%llu, ITER:%d\n",
            perfdata_avg(&pd[cos_cpuid()]), perfdata_max(&pd[cos_cpuid()]), perfdata_min(&pd[cos_cpuid()]), perfdata_sz(&pd[cos_cpuid()]));      

    printc("\t\t\t\t\t\t\tSD:%llu, 90%%:%llu, 95%%:%llu, 99%%:%llu\n",
            perfdata_sd(&pd[cos_cpuid()]),perfdata_90ptile(&pd[cos_cpuid()]), perfdata_95ptile(&pd[cos_cpuid()]), perfdata_99ptile(&pd[cos_cpuid()]));

    perfdata_init(&pd[cos_cpuid()], "COS THD => COS_SWITCH");

    for (i = 0; i < ITER; i++) {
        rdtscll(main_thd);
        ret = cos_switch(ts, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, 0, 0, 0, 0);
        EXPECT_LL_NEQ(0, ret, "COS Switch Error");

        perfdata_add(&pd[cos_cpuid()], (side_thd - main_thd));
    }

    perfdata_calc(&pd[cos_cpuid()]);

    PRINTC("\tCOS THD => COS_SWITCH:\t\t\tAVG:%llu, MAX:%llu, MIN:%llu, ITER:%d\n",
            perfdata_avg(&pd[cos_cpuid()]), perfdata_max(&pd[cos_cpuid()]), perfdata_min(&pd[cos_cpuid()]), perfdata_sz(&pd[cos_cpuid()]));      

    printc("\t\t\t\t\t\t\tSD:%llu, 90%%:%llu, 95%%:%llu, 99%%:%llu\n",
            perfdata_sd(&pd[cos_cpuid()]),perfdata_90ptile(&pd[cos_cpuid()]), perfdata_95ptile(&pd[cos_cpuid()]), perfdata_99ptile(&pd[cos_cpuid()]));
}

static void
async_thd_fn_perf(void *thdcap)
{
    thdcap_t  tc = (thdcap_t)thdcap;
    asndcap_t sc = scc_global[cos_cpuid()];
    arcvcap_t rc = rcc_global[cos_cpuid()];
    int       i, ret, pending = 0;

    for (i = 0; i < ITER; i++) {
        cos_rcv(rc, 0, NULL);
        cos_asnd(sc, 1);
    }

    cos_thd_switch(tc);

    for (i = 0; i < ITER + 1; i++) {
        cos_rcv(rc, 0, NULL);
    }

    ret = cos_thd_switch(tc);
    EXPECT_LL_NEQ(0, ret, "COS Switch Error");
}

static void
async_thd_parent_perf(void *thdcap)
{
    thdcap_t  tc = (thdcap_t)thdcap;
    asndcap_t sc = scp_global[cos_cpuid()];
    arcvcap_t rc = rcc_global[cos_cpuid()];
    long long e = 0, s = 0;
    int       i, pending = 0;

    perfdata_init(&pd[cos_cpuid()], "Async Endpoints => Roundtrip");

    for (i = 0; i < ITER; i++) {
        rdtscll(s);
        cos_asnd(sc, 1);
        cos_rcv(rc, 0, NULL);
        rdtscll(e);

        perfdata_add(&pd[cos_cpuid()], (e - s));
    }

    perfdata_calc(&pd[cos_cpuid()]);

    PRINTC("\tAsync Endpoints => Roundtrip:\t\tAVG:%llu, MAX:%llu, MIN:%llu, ITER:%d\n",
            perfdata_avg(&pd[cos_cpuid()]), perfdata_max(&pd[cos_cpuid()]), perfdata_min(&pd[cos_cpuid()]),
            perfdata_sz(&pd[cos_cpuid()]));      

    printc("\t\t\t\t\t\t\tSD:%llu, 90%%:%llu, 95%%:%llu, 99%%:%llu\n",
            perfdata_sd(&pd[cos_cpuid()]),perfdata_90ptile(&pd[cos_cpuid()]), perfdata_95ptile(&pd[cos_cpuid()]),
            perfdata_99ptile(&pd[cos_cpuid()]));

    perfdata_init(&pd[cos_cpuid()], "Async Endpoints => One Way");

    for (i = 0; i < ITER; i++) {
        rdtscll(s);
        cos_asnd(sc, 1);
        rdtscll(e);

        perfdata_add(&pd[cos_cpuid()], (e - s));
    }

    perfdata_calc(&pd[cos_cpuid()]);

    PRINTC("\tAsync Endpoints => One Way:\t\tAVG:%llu, MAX:%llu, MIN:%llu, ITER:%d\n",
            perfdata_avg(&pd[cos_cpuid()]), perfdata_max(&pd[cos_cpuid()]), perfdata_min(&pd[cos_cpuid()]),
            perfdata_sz(&pd[cos_cpuid()]));      

    printc("\t\t\t\t\t\t\tSD:%llu, 90%%:%llu, 95%%:%llu, 99%%:%llu\n",
            perfdata_sd(&pd[cos_cpuid()]),perfdata_90ptile(&pd[cos_cpuid()]), perfdata_95ptile(&pd[cos_cpuid()]),
            perfdata_99ptile(&pd[cos_cpuid()]));

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
    perfdata_calc(&result_sinv);

    PRINTC("\tSINV:\t\t\t\t\tAVG:%llu, MAX:%llu, MIN:%llu, ITER:%d\n",
            perfdata_avg(&result_sinv), perfdata_max(&result_sinv), perfdata_min(&result_sinv),
            perfdata_sz(&result_sinv));      

    printc("\t\t\t\t\t\t\tSD:%llu, 90%%:%llu, 95%%:%llu, 99%%:%llu\n",
            perfdata_sd(&result_sinv),perfdata_90ptile(&result_sinv), perfdata_95ptile(&result_sinv),
            perfdata_99ptile(&result_sinv));

    perfdata_calc(&result_test_timer);

    PRINTC("\tTimer => Timeout Overhead: \t\tAVG:%llu, MAX:%llu, MIN:%llu, ITER:%d\n",
            perfdata_avg(&result_test_timer), perfdata_max(&result_test_timer), perfdata_min(&result_test_timer),
            perfdata_sz(&result_test_timer));      

    printc("\t\t\t\t\t\t\tSD:%llu, 90%%:%llu, 95%%:%llu, 99%%:%llu\n",
            perfdata_sd(&result_test_timer),perfdata_90ptile(&result_test_timer), perfdata_95ptile(&result_test_timer),
            perfdata_99ptile(&result_test_timer));

    perfdata_calc(&result_budgets_single);

    PRINTC("\tTimer => Budget based: \t\t\tAVG:%llu, MAX:%llu, MIN:%llu, ITER:%d\n",
            perfdata_avg(&result_budgets_single), perfdata_max(&result_budgets_single), perfdata_min(&result_budgets_single),
            perfdata_sz(&result_budgets_single));      

    printc("\t\t\t\t\t\t\tSD:%llu, 90%%:%llu, 95%%:%llu, 99%%:%llu\n",
            perfdata_sd(&result_budgets_single),perfdata_90ptile(&result_budgets_single), perfdata_95ptile(&result_budgets_single),
            perfdata_99ptile(&result_budgets_single));
}

void
test_run_perf_mb(void)
{
    cyc_per_usec = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);
    test_thds_create_switch();
    test_async_endpoints_perf();
    test_print_ubench();
}

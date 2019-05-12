/*
 * Testing cos_rcv and cos_asnd
 * The test uses 2 thds, ine that sends and one that rcvs
 * asend with a 0 ARG is NON_BLOCKING.
 */

#include <stdint.h>
#include "kernel_tests.h"

static volatile arcvcap_t rcc_global[NUM_CPU], rcp_global[NUM_CPU];
static volatile asndcap_t scp_global[NUM_CPU];
static int      async_test_flag_[NUM_CPU] = { 0 };
static int      failure = 0;

#define TEST_TIMEOUT_MS 1

static void
async_thd_fn(void *thdcap)
{
        thdcap_t  tc = (thdcap_t)thdcap;
        arcvcap_t rc = rcc_global[cos_cpuid()];
        int       pending, rcvd, ret;

        pending = cos_rcv(rc, RCV_NON_BLOCKING, NULL);
        if (EXPECT_LL_NEQ(3, pending, "Test Async Endpoints")) failure = 1;

        pending = cos_rcv(rc, RCV_NON_BLOCKING | RCV_ALL_PENDING, &rcvd);
        if (EXPECT_LL_NEQ(0, pending, "Test Async Endpoints")) failure = 1;

        pending = cos_rcv(rc, RCV_ALL_PENDING, &rcvd);
        /* switch */
        if (EXPECT_LL_NEQ(0, pending, "Test Async Endpoints")) failure = 1;

        pending = cos_rcv(rc, 0, NULL);
        /* switch */
        if (EXPECT_LL_NEQ(0, pending, "Test Async Endpoints")) failure = 1;

        pending = cos_rcv(rc, 0, NULL);
        /* switch */
        if (EXPECT_LL_NEQ(0, pending, "Test Async Endpoints")) failure = 1;

        pending = cos_rcv(rc, RCV_NON_BLOCKING, NULL);
        if (EXPECT_LL_NEQ(pending, -EAGAIN, "Test Async Endpoints")) failure = 1;

        pending = cos_rcv(rc, 0, NULL);
        /* switch */
        if (EXPECT_LL_NEQ(0, 1, "Test Async Endpoints")) failure = 1;

        ret = cos_thd_switch(tc);
        if (EXPECT_LL_NEQ(0, ret, "COS Switch Error") ||
                EXPECT_LL_NEQ(0, 1, "Test Async Endpoints")) {
                failure = 1;
        }
        while (1) cos_thd_switch(tc);
}

static void
async_thd_parent(void *thdcap)
{
        thdcap_t    tc = (thdcap_t)thdcap;
        arcvcap_t   rc = rcp_global[cos_cpuid()];
        asndcap_t   sc = scp_global[cos_cpuid()];
        int         ret;
        thdid_t     tid;
        int         blocked, rcvd;
        cycles_t    cycles, now;
        tcap_time_t thd_timeout;

        /* NON_BLOCKING ASND with 0 as arg*/
        ret = cos_asnd(sc, 0);
        ret = cos_asnd(sc, 0);
        ret = cos_asnd(sc, 0);
        ret = cos_asnd(sc, 1);

        /* switch */
        /* child blocked at this point, parent is using child's tcap, this call yields to the child */
        ret = cos_asnd(sc, 0);

        /* switch */
        ret = cos_asnd(sc, 0);
        if (EXPECT_LL_NEQ(0, ret, "Test Async Endpoints")) failure = 1;

        /* switch */
        ret = cos_asnd(sc, 1);
        if (EXPECT_LL_NEQ(0, ret, "Test Async Endpoints")) failure = 1;

        /* switch */
        cos_sched_rcv(rc, RCV_ALL_PENDING, 0, &rcvd, &tid, &blocked, &cycles, &thd_timeout);
        rdtscll(now);

        async_test_flag_[cos_cpuid()] = 0;
        while (1) cos_thd_switch(tc);
}

void
test_async_endpoints(void)
{
        thdcap_t  tcp, tcc;
        tcap_t    tccp, tccc;
        arcvcap_t rcp, rcc;
        asndcap_t scr;

        /* parent rcv capabilities */
        tcp = cos_thd_alloc(&booter_info, booter_info.comp_cap, async_thd_parent,
                            (void *)BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);
        if (EXPECT_LL_LT(1, tcp, "Test Async Endpoints")) {
                return;
        }
        tccp = cos_tcap_alloc(&booter_info);
        if (EXPECT_LL_LT(1, tccp, "Test Async Endpoints")) {
                return;
        }
        rcp = cos_arcv_alloc(&booter_info, tcp, tccp, booter_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE);
        if (EXPECT_LL_LT(1, rcp, "Test Async Endpoints")) {
                return;
        }
        if (EXPECT_LL_NEQ(0,cos_tcap_transfer(rcp, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, TCAP_PRIO_MAX),
                                              "Test Async Endpoints")) {
                return;
        }

        /* child rcv capabilities */
        tcc = cos_thd_alloc(&booter_info, booter_info.comp_cap, async_thd_fn, (void *)tcp);
        if (EXPECT_LL_LT(1, tcc, "Test Async Endpoints")) {
                return;
        }
        tccc = cos_tcap_alloc(&booter_info);
        if (EXPECT_LL_LT(1, tccc, "Test Async Endpoints")) {
                return;
        }
        rcc = cos_arcv_alloc(&booter_info, tcc, tccc, booter_info.comp_cap, rcp);
        if (EXPECT_LL_LT(1, rcc, "Test Async Endpoints")) {
                return;
        }
        if (EXPECT_LL_NEQ(0,cos_tcap_transfer(rcc, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF,
                                         TCAP_PRIO_MAX + 1), "Test Async Endpoints")) {
                return;
        }

        /* make the snd channel to the child */
        scp_global[cos_cpuid()] = cos_asnd_alloc(&booter_info, rcc, booter_info.captbl_cap);
        if (EXPECT_LL_EQ(0, scp_global[cos_cpuid()], "Test Async Endpoints")) return;
        scr = cos_asnd_alloc(&booter_info, rcp, booter_info.captbl_cap);
        if (EXPECT_LL_EQ(0, scr, "Test Async Endpoints")) return;

        rcc_global[cos_cpuid()] = rcc;
        rcp_global[cos_cpuid()] = rcp;

        async_test_flag_[cos_cpuid()] = 1;
        while (async_test_flag_[cos_cpuid()]) cos_asnd(scr, 1);

        CHECK_STATUS_FLAG();
        PRINTC("\t%s: \t\tSuccess\n", "Asynchronous Endpoints");
        EXIT_FN();
}

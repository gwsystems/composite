#include <stdint.h>
#include "kernel_tests.h"

static int          failure = 0;

/*
 * Fundamental check & test
 * checks thd creation, arg passing and basic swich
 */

static void
test_thd_arg(void *d)
{
        int ret = 0;

        if (EXPECT_LL_NEQ((int)d, THD_ARG, "Thread Creation: Argument Incorrect")) failure = 1;
        while (1) cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);
        PRINTC("Error, shouldn't get here!\n");
}

static void
test_thds_create_switch(void)
{
        thdcap_t ts;
        intptr_t i = THD_ARG;
        int      ret;

        ts = cos_thd_alloc(&booter_info, booter_info.comp_cap, test_thd_arg, (void *)i);
        if (EXPECT_LL_LT(1, ts, "Thread Creation: Cannot Allocate")) {
                return;
        }
        ret = cos_thd_switch(ts);
        EXPECT_LL_NEQ(0, ret, "COS Switch Error");

        CHECK_STATUS_FLAG();
        PRINTC("\t%s: \t\t\tSuccess\n", "THD => Creation & ARG");
        EXIT_FN();
}

static void
thd_fn_mthds_ring(void *d)
{
        int ret;

        if (count != (int) d) cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);

        int next = (++count) % TEST_NTHDS;
        if (!next) cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);

        ret = cos_thd_switch(thd_test[next]);
        if (EXPECT_LL_NEQ(0, ret, "Thread Ring: COS Switch Error")) failure = 1;

        while (1) {
                cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);
        }
        EXPECT_LL_NEQ(1, 0, "Error, shouldn't get here!\n");
        assert(0);
}

/* Ring Multithreaded Test
 * Bounce between TEST_NTHDS in a circular fashion, a running thd references
 * it's right neighbord and switches until it switches back to the main thd
 * while checking the run order
 */

static void
test_mthds_ring(void)
{
        int   i, ret;

        count = 0;

        for (i = 0; i < TEST_NTHDS; i++) {
                thd_test[i] = cos_thd_alloc(&booter_info, booter_info.comp_cap, thd_fn_mthds_ring, (void *)i);
                if (EXPECT_LL_LT(1, thd_test[i], "Thread Ring: Cannot Allocate")) {
                        return;
                }
        }

        ret = cos_thd_switch(thd_test[0]);
        EXPECT_LL_NEQ(0, ret, "Thread Ring: COS Switch Error");

        if (EXPECT_LL_NEQ(count, TEST_NTHDS, "Thread Ring: Failure # of THDS")) {
                return;
        }

        CHECK_STATUS_FLAG();
        PRINTC("\t%s: \t\t\tSuccess\n", "THD => Switch Cyclic" );
        EXIT_FN();
}

static void
thd_fn_mthds_classic(void *d)
{
        cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);

        while (1) {
                cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);
        }
        EXPECT_LL_NEQ(1, 0, "Error, shouldn't get here!\n");
        assert(0);
}

/* Classic Multithreaded Test
 * The thds will switch between the main thd and it each individual thd back and forth.
 * testing if it switched to the desirable thd at each iteration
 */

static void
test_mthds_classic(void)
{
        thdcap_t  ts;
        int       i, ret;

        ts = cos_thd_alloc(&booter_info, booter_info.comp_cap, thd_fn_mthds_classic, NULL);
        if (EXPECT_LL_LT(1, ts, "Thread Classic: Cannot Allocate")) {
                return;
        }

        for (i = 0; i < ITER; i++) {
                ret = cos_thd_switch(ts);
                if(EXPECT_LL_NEQ(0, ret, "Thread Classic: COS Switch Error")) return;
        }
        CHECK_STATUS_FLAG();
        PRINTC("\t%s: \t\tSuccess\n", "THD => Switch in pairs");
        EXIT_FN();
}

static void
thd_tls(void *d)
{
        if (EXPECT_LLU_NEQ((long unsigned)tls_get(0), (long unsigned)tls_test[cos_cpuid()][(int)d],
                            "Thread TLS: ARG not correct")) failure = 1;
        while (1) cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);
        EXPECT_LL_NEQ(1, 0, "Error, shouldn't get here!\n");
        assert(0);
}

/* Test the TLS support
 * Sets a TLS value and check if it was set and passed to the thd
 */
static void
test_thds_tls(void)
{
        thdcap_t ts[TEST_NTHDS];
        intptr_t i;
        int      ret;

        for (i = 0; i < TEST_NTHDS; i++) {
                ts[i] = cos_thd_alloc(&booter_info, booter_info.comp_cap, thd_tls, (void *)i);
                if (EXPECT_LL_LT(1, ts[i], "Thread TLS: Cannot Allocate")) {
                        return;
                }
                tls_test[cos_cpuid()][i] = i;
                cos_thd_mod(&booter_info, ts[i], &tls_test[cos_cpuid()][i]);
                ret = cos_thd_switch(ts[i]);
                if (EXPECT_LL_NEQ(0, ret, "Thread TLS: COS Switch Error")) return;
        }

        CHECK_STATUS_FLAG();
        PRINTC("\t%s: \t\t\tSuccess\n", "THD => Creation & TLS");
        EXIT_FN();
}

void
test_thds(void)
{
        test_thds_create_switch();
        test_thds_tls();
        test_mthds_classic();
        test_mthds_ring();
}


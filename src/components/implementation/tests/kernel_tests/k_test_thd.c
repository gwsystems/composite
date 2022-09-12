#include <stdint.h>
#include "kernel_tests.h"

static int              failure = 0;
static thdcap_t         thds[4];
/*
 * Fundamental check & test
 * checks thd creation, arg passing and basic swich
 */

static void
test_thd_arg(void *d)
{
        int ret = 0;

        if (EXPECT_LL_NEQ((word_t)d, THD_ARG, "Thread Creation: Argument Incorrect")) failure = 1;
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

        if (count != (word_t)d) cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);

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
        word_t	i;
        int	ret;

        count	= 0;

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
#if defined(__x86__)
        if (EXPECT_LLU_NEQ((long unsigned)tls_get(0), (long unsigned)tls_test[cos_cpuid()][(int)d],
                            "Thread TLS: ARG not correct")) failure = 1;
        while (1) cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);
        EXPECT_LL_NEQ(1, 0, "Error, shouldn't get here!\n");
        assert(0);
#endif
}

/* Test the TLS support
 * Sets a TLS value and check if it was set and passed to the thd
 */
static void
test_thds_tls(void)
{
#if defined(__x86__)
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
#endif
}

static void
thds_fpu(void *d)
{
        float    PI   = 3.0;
        int      flag = 1;
        word_t      i;
        for (i = 2; i < 20; i += 2) {	
                if (flag) {
                        PI += (4.0 / (i * (i + 1) * (i + 2)));
                } else {
                        PI -= (4.0 / (i * (i + 1) * (i + 2)));
                }
                if ((word_t) d == 1) {
                        cos_thd_switch(thds[2]);
                } else if ((word_t) d == 2) {
                        cos_thd_switch(thds[1]);		
                } else if ((word_t) d == 3) {
                        cos_thd_switch(thds[0]);		
                }
                flag = !flag;
        }
        if ((int)PI != 3) PRINTC("\t%s: \t\t\tPI = %f ERROR\n", "PI VALUE", PI);
        cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);
        return;
}

static void
test_thds_reg(void *d)
{
        int ret = 0;
        while (1) cos_thd_switch(thds[3]);
        PRINTC("Error, shouldn't get here!\n");
}

static void
test_thds_fpu(void)
{
        intptr_t i = 0;
        int      ret;
        
        thds[0] = cos_thd_alloc(&booter_info, booter_info.comp_cap, test_thds_reg, (void *)i);
        for (i = 1; i <= 3; i++) {
                thds[i] = cos_thd_alloc(&booter_info, booter_info.comp_cap, thds_fpu, (void *)i);
                if (EXPECT_LL_LT(1, thds[i], "Thread FPU: Cannot Allocate")) {
                        return;
                }
        }
        for (i = 0; i < 3; i++) {
                ret = cos_thd_switch(thds[i]);
                if (EXPECT_LL_NEQ(0, ret, "Thread FPU: COS Switch Error")) return;
        }
        CHECK_STATUS_FLAG();
        PRINTC("\t%s: \t\t\tSuccess\n", "THD => FPU Thd Switch");
        EXIT_FN();
}

void
test_thds(void)
{
        test_thds_create_switch();
        test_thds_tls();
        test_mthds_classic();
        test_mthds_ring();
        test_thds_fpu();
}



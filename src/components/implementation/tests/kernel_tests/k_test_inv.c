/*
 * Test allocates a tcap_invocation anf proceeds to invoke it
 * It additionally measures the roundtrip time
 */

#include <stdint.h>
#include "kernel_tests.h"

static int      failure = 0;
struct perfdata result;
struct results  result_sinv;

#define ARRAY_SIZE 10000
static cycles_t test_results[ARRAY_SIZE] = { 0 };

int
test_serverfn(int a, int b, int c)
{
        return 0xDEADBEEF;
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

void
test_inv(void)
{
        compcap_t        cc;
        sinvcap_t        ic;
        unsigned int r;
        int                  i;
        cycles_t         start_cycles = 0LL, end_cycles = 0LL;

        perfdata_init(&result, "SINV", test_results, ARRAY_SIZE);

        cc = cos_comp_alloc(&booter_info, booter_info.captbl_cap, booter_info.pgtbl_cap, (vaddr_t)NULL);
        if (EXPECT_LL_LT(1, cc, "Invocation: Cannot Allocate")) return;
        ic = cos_sinv_alloc(&booter_info, cc, (vaddr_t)__inv_test_serverfn, 0);
        if (EXPECT_LL_LT(1, ic, "Invocation: Cannot Allocate")) return;

        r = call_cap_mb(ic, 1, 2, 3);
        if (EXPECT_LLU_NEQ(0xDEADBEEF, r, "Test Invocation")) return;

        for (i = 0; i < ITER; i++) {
                rdtscll(start_cycles);
                call_cap_mb(ic, 1, 2, 3);
                rdtscll(end_cycles);

                perfdata_add(&result, end_cycles - start_cycles);
        }

        perfdata_calc(&result);
        result_sinv.avg = perfdata_avg(&result);
        result_sinv.max = perfdata_avg(&result);
        result_sinv.min = perfdata_avg(&result);
        result_sinv.sz = perfdata_avg(&result);
        result_sinv.sd = perfdata_avg(&result);
        result_sinv.p90tile = perfdata_avg(&result);
        result_sinv.p95tile = perfdata_avg(&result);
        result_sinv.p99tile = perfdata_avg(&result);

        CHECK_STATUS_FLAG();
        PRINTC("\t%s: \t\tSuccess\n", "Synchronous Invocations");
        EXIT_FN();
}

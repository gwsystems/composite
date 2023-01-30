/*
 * Test allocates a new component, then proceeds to allocate a new invocation
 * the allocation cos_sinv_alloc is checked
 */

#include <stdint.h>
#include "kernel_tests.h"

#define CAPTBL_ITER 1024

extern int test_serverfn(int a, int b, int c);
extern void *__inv_test_serverfn(int a, int b, int c);

void
test_captbl_expands(void)
{
        int       i;
        compcap_t cc;

        cc = cos_comp_alloc(&booter_info, booter_info.captbl_cap, booter_info.pgtbl_cap, (vaddr_t)NULL, 0);
        if (EXPECT_LL_LT(1, cc, "Capability Table Expansion")) {
                return;
        }
        for (i = 0; i < CAPTBL_ITER; i++) {
                sinvcap_t ic;

                ic = cos_sinv_alloc(&booter_info, cc, (vaddr_t)__inv_test_serverfn, 0);
                if(EXPECT_LL_LT(1, ic, "Capability Table: Cannot Allocate")) {
                        return;
                }
        }
        PRINTC("\t%s: \t\tSuccess\n", "Capability Table Expansion");
}

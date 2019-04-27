/*
 * Test allocates pages through cos_page_vumo_alloc
 * then proceeds to strcpy and memcpy into it
 * Additionally it maps more anf check that the pages are contiguous in memoery
 */

#include <stdint.h>
#include "kernel_tests.h"

#define TEST_NPAGES (1024 * 2)          /* Testing with 8MB for now */

/* Executed in micro_booter environment */
void
test_mem_alloc(void)
{
        char *      p, *s, *t, *prev;
        int         i;
        const char *chk = "SUCCESS";
        int         fail_contiguous = 0;

        p = cos_page_bump_alloc(&booter_info);
        if (p == NULL) {
                EXPECT_LL_NEQ(0, 1, "Memory Test: Cannot Allocate");
                return;
        }
        PRINTC("\t%s: \t\t\tSuccess\n", "Memory => Allocation");
        strcpy(p, chk);

        if (EXPECT_LL_NEQ(0, strcmp(chk, p), "Memory Test: Wrong STRCPY")) {
                return;
        }

        s = cos_page_bump_alloc(&booter_info);
        assert(s);
        prev = s;
        for (i = 0; i < TEST_NPAGES; i++) {
                t = cos_page_bump_alloc(&booter_info);
                if (t == NULL){
                        EXPECT_LL_EQ(0, 1, "Memory Test: Cannot Allocate");
                        return;
                }
                if (t != prev + PAGE_SIZE) {
                        fail_contiguous = 1;
                }
                prev = t;
        }
        if (!fail_contiguous) {
                memset(s, 0, TEST_NPAGES * PAGE_SIZE);
        } else if (EXPECT_LL_EQ(i, TEST_NPAGES,"Memory Test: Cannot Allocate contiguous")) {
                return;
        }

        t = cos_page_bump_allocn(&booter_info, TEST_NPAGES * PAGE_SIZE);
        if (t == NULL) {
                EXPECT_LL_NEQ(0, 1, "Memory Test: Cannot Allocate");
                return;
        }
        memset(t, 0, TEST_NPAGES * PAGE_SIZE);
        PRINTC("\t%s: \t\t\tSuccess\n", "Memory => R & W");
}

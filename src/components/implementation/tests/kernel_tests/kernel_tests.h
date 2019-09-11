#ifndef KERNEL_TESTS_H
#define KERNEL_TESTS_H

#include <stdio.h>
#include <string.h>

#include <cos_debug.h>
#include <llprint.h>

#undef assert
/* On assert, immediately switch to the "exit" thread */
#define assert(node)                                                        \
        do {                                                                \
                if (unlikely(!(node))) {                                    \
                        debug_print("assert error in @ ");                  \
                        cos_thd_switch(termthd[cos_cpuid()]);               \
                }                                                           \
        } while (0)

#define EXIT_FN()                                                           \
                exit_fn: return;

#define CHECK_STATUS_FLAG()                                                 \
        do {                                                                \
                if (failure) {                                              \
                        goto exit_fn;                                       \
                }                                                           \
        } while (0)

#include <cos_component.h>
#include <cobj_format.h>
#include <cos_kernel_api.h>
#include <perfdata.h>
#include <cos_ubench.h>

#define PERF
#define ITER 10000
#define TEST_NTHDS 5
#define CHAR_BIT 8
#define TEST_ITER 16

#define THD_ARG 666             /* Thread Argument to pass */
#define NUM_TEST 16             /* Iterator NUM */
#define MAX_THDS 4              /* Max Threshold Multiplier */
#define MIN_THDS 0.5            /* Min Threshold Multiplier */
#define GRANULARITY 1000        /* Granularity */

#define TEST_NPAGES (1024 * 2)  /* Testing with 8MB for now */

unsigned int cyc_per_usec;

extern struct cos_compinfo booter_info;
extern thdcap_t         termthd[]; /* switch to this to shutdown */
extern unsigned long    tls_test[][TEST_NTHDS];
extern unsigned long    thd_test[TEST_NTHDS];
extern int              num, den, count;

struct results {
        long long unsigned avg;
        long long unsigned max;
        long long unsigned min;
        long long unsigned sd;
        int                sz;
        long long unsigned p90tile;
        long long unsigned p95tile;
        long long unsigned p99tile;
};

static unsigned long
tls_get(size_t off)
{
        unsigned long val;

        __asm__ __volatile__("movl %%gs:(%1), %0" : "=r"(val) : "r"(off) :);

        return val;
}

static void
tls_set(size_t off, unsigned long val)
{
        __asm__ __volatile__("movl %0, %%gs:(%1)" : : "r"(val), "r"(off) : "memory");
}

extern void test_run_perf_kernel(void);
extern void test_timer(void);
extern void test_tcap_budgets(void);
extern void test_2timers(void);
extern void test_thds(void);
extern void test_mem_alloc(void);
extern void test_async_endpoints(void);
extern void test_inv(void);
extern void test_captbl_expands(void);

#endif /* KERNEL_TESTS_H */

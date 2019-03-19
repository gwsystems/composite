#ifndef MICRO_XCORES_H
#define MICRO_XCORES_H

#include <stdio.h>
#include <string.h>

#include <cos_debug.h>
#include <llprint.h>

#undef assert
/* On assert, immediately switch to the "exit" thread */
#define assert(node)                                          \
    do {                                                  \
        if (unlikely(!(node))) {                      \
            debug_print("assert error in @ ");    \
            cos_thd_switch(termthd[cos_cpuid()]); \
        }                                             \
    } while (0)

#undef EXPECT_LLU
#define EXPECT_LLU(boole ,printstr, errcmp, a, b, name) \
    _expect_llu((boole), (": " #a " " errcmp " " #b " (evaluated to"), a, b, errcmp, name, __FILE__, __LINE__)

#define EXPECT_LLU_NEQ(a, b, name) EXPECT_LLU((a) != (b), "%llu",  "==", (a), (b), name)

#define EXPECT_LLU_LT(a, b, name) EXPECT_LLU((a) > (b), "%llu",  "<=", (a), (b), name)

#undef EXPECT_LL
#define EXPECT_LL(boole ,printstr, errcmp, a, b, name) \
    _expect_ll((boole), (": " #a " " errcmp " " #b " (evaluated to"), a, b, errcmp, name, __FILE__, __LINE__)

#define EXPECT_LL_NEQ(a, b, name) EXPECT_LL((a) != (b), "%lld",  "==", (a), (b), name)

#define EXPECT_LL_EQ(a, b, name) EXPECT_LL((a) == (b), "%lld",  "!=", (a), (b), name)

#define EXPECT_LL_LT(a, b, name) EXPECT_LL((a) > (b), "%lld",  "<=", (a), (b), name)

#define BUG_DIVZERO()                                           \
    do {                                                    \
        debug_print("Testing divide by zero fault @ "); \
        int i = num / den;                              \
    } while (0);

#include <cos_component.h>
#include <cobj_format.h>
#include <cos_kernel_api.h>

#define ITER 10000
#define TEST_NTHDS 5
#define CHAR_BIT 8

#define TEST_RCV_CORE 0
#define TEST_SND_CORE 1
#define TEST_IPI_ITERS 10000

extern struct cos_compinfo booter_info;
extern thdcap_t            termthd[]; /* switch to this to shutdown */

extern void test_ipi_n_n(void);
extern void test_ipi_interference(void);
extern void test_ipi_switch(void);
extern void test_ipi_roundtrip(void);

#endif /* MICRO_XCORES_H */

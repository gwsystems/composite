#ifndef MICRO_BOOTER_H
#define MICRO_BOOTER_H

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

extern struct cos_compinfo booter_info;
extern thdcap_t            termthd[]; /* switch to this to shutdown */
extern unsigned long       tls_test[][TEST_NTHDS];
extern int                 num, den;

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

extern void test_run_mb(void);
extern void test_ipi_full(void);

#endif /* MICRO_BOOTER_H */

#ifndef MICRO_BOOTER_H
#define MICRO_BOOTER_H

#include <stdio.h>
#include <string.h>

#include <cos_component.h>
#include <cobj_format.h>
#include <cos_kernel_api.h>

#include "vk_types.h"

#define debug_print(str) (PRINT_FN(str __FILE__ ":" STR(__LINE__) ".\n"))
#define BUG_DIVZERO()                                           \
	do {                                                    \
		debug_print("Testing divide by zero fault @ "); \
		int i = num / den;                              \
	} while (0)


#undef PRINTC
#define PRINTC(fmt, args...) printc("%d: " fmt, vmid, ##args)
#define ITER 10000
#define TEST_NTHDS 5

extern struct cos_compinfo booter_info;
extern thdcap_t            termthd[]; /* switch to this to shutdown */
extern unsigned long       tls_test[][TEST_NTHDS];
extern int                 num, den;
extern int                 vmid;

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

extern void test_run_vk(void);

static void
test_ipi_full(void)
{
}

#endif /* MICRO_BOOTER_H */

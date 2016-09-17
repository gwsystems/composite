#ifndef MICRO_BOOTER_H
#define MICRO_BOOTER_H
#include <stdio.h>
#include <string.h>

#undef assert
#ifndef assert
/* On assert, immediately switch to the "exit" thread */
#define assert(node) do { if (unlikely(!(node))) { debug_print("assert error in @ "); EXIT(); } } while(0)
#endif

#define PRINT_FN prints
#define debug_print(str) (PRINT_FN(str __FILE__ ":" STR(__LINE__) ".\n"))
#define BUG_DIVZERO() do { debug_print("Testing divide by zero fault @ "); int i = num / den; } while (0)
#define EXIT() do { while (1) cos_thd_switch(termthd); } while (0);

#include <cos_component.h>
#include <cobj_format.h>
#include <cos_kernel_api.h>

#include "vk_types.h"

#define PRINTC(fmt, args...) printc("%d: " fmt, vmid , ##args)
#define ITER       10000
#define TEST_NTHDS 5

extern struct cos_compinfo booter_info;
extern thdcap_t termthd; 		/* switch to this to shutdown */
extern unsigned long tls_test[TEST_NTHDS];
extern int num, den;
extern int vmid;

static unsigned long
tls_get(size_t off)
{
	unsigned long val;

	__asm__ __volatile__("movl %%gs:(%1), %0" : "=r" (val) : "r" (off) : );

	return val;
}

static void
tls_set(size_t off, unsigned long val)
{ __asm__ __volatile__("movl %0, %%gs:(%1)" : : "r" (val), "r" (off) : "memory"); }

extern int prints(char *s);
extern int printc(char *fmt, ...);
extern void test_run(void);

#endif /* MICRO_BOOTER_H */

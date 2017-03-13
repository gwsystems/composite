/*
 * Copyright 2016, Phani Gadepalli and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <stdio.h>
#include <string.h>
#include <cos_component.h>
#include <cobj_format.h>
#include <cos_defkernel_api.h>

#include <sl.h>

#undef assert
#define assert(node) do { if (unlikely(!(node))) { debug_print("assert error in @ "); *((int *)0) = 0; } } while (0)
#define PRINT_FN prints
#define debug_print(str) (PRINT_FN(str __FILE__ ":" STR(__LINE__) ".\n"))
#define BUG() do { debug_print("BUG @ "); *((int *)0) = 0; } while (0);
#define SPIN(iters) do { if (iters > 0) { for (; iters > 0 ; iters -- ) ; } else { while (1) ; } } while (0)

static void
cos_llprint(char *s, int len)
{ call_cap(PRINT_CAP_TEMP, (int)s, len, 0, 0); }

int
prints(char *s)
{
	int len = strlen(s);

	cos_llprint(s, len);

	return len;
}

int __attribute__((format(printf,1,2)))
printc(char *fmt, ...)
{
	  char s[128];
	  va_list arg_ptr;
	  int ret, len = 128;

	  va_start(arg_ptr, fmt);
	  ret = vsnprintf(s, len, fmt, arg_ptr);
	  va_end(arg_ptr);
	  cos_llprint(s, ret);

	  return ret;
}

#define N_TESTTHDS 8
#define WORKITERS  10000

void
test_thd_fn(void *data)
{
	while (1) {
		int workiters = WORKITERS * ((int)data);

		SPIN(workiters);
		sl_thd_yield(0);
	}
}

void
cos_init(void)
{
	int                     i;
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci    = cos_compinfo_get(defci);
	struct sl_thd          *threads[N_TESTTHDS];
	union sched_param       sp    = {.c = {.type = SCHEDP_PRIO, .value = 10}};

	printc("Unit-test for the scheduling library (sl)\n");
	cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_defcompinfo_init();
	sl_init();

	for (i = 0 ; i < N_TESTTHDS ; i++) {
		threads[i] = sl_thd_alloc(test_thd_fn, (void *)(i + 1));
		assert(threads[i]);
		sl_thd_param_set(threads[i], sp.v);
	}

	sl_sched_loop();

	assert(0);

	return;
}

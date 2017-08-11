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
#define assert(node)                                       \
	do {                                               \
		if (unlikely(!(node))) {                   \
			debug_print("assert error in @ "); \
			*((int *)0) = 0;                   \
		}                                          \
	} while (0)
#define PRINT_FN prints
#define debug_print(str) (PRINT_FN(str __FILE__ ":" STR(__LINE__) ".\n"))
#define BUG()                          \
	do {                           \
		debug_print("BUG @ "); \
		*((int *)0) = 0;       \
	} while (0);
#define SPIN(iters)                                \
	do {                                       \
		if (iters > 0) {                   \
			for (; iters > 0; iters--) \
				;                  \
		} else {                           \
			while (1)                  \
				;                  \
		}                                  \
	} while (0)

#include <llprint.h>

#define N_TESTTHDS 8
#define WORKITERS 10000

void
test_thd_fn(void *data)
{
	while (1) {
		int workiters = WORKITERS * ((int)data);

		printc("%d", (int)data);
		SPIN(workiters);
		sl_thd_yield(0);
	}
}

void
test_yields(void)
{
	int               i;
	struct sl_thd *   threads[N_TESTTHDS];
	union sched_param sp = { .c = { .type = SCHEDP_PRIO, .value = 10 } };

	for (i = 0; i < N_TESTTHDS; i++) {
		threads[i] = sl_thd_alloc(test_thd_fn, (void *)(i + 1));
		assert(threads[i]);
		sl_thd_param_set(threads[i], sp.v);
	}
}

void
test_high(void *data)
{
	struct sl_thd *t = data;

	while (1) {
		sl_thd_yield(t->thdid);
		printc("h");
	}
}

void
test_low(void *data)
{
	while (1) {
		int workiters = WORKITERS * 10;
		SPIN(workiters);
		printc("l");
	}
}

void
test_blocking_directed_yield(void)
{
	struct sl_thd *   low, *high;
	union sched_param sph = { .c = { .type = SCHEDP_PRIO, .value = 5 } };
	union sched_param spl = { .c = { .type = SCHEDP_PRIO, .value = 10 } };

	low  = sl_thd_alloc(test_low, NULL);
	high = sl_thd_alloc(test_high, low);
	sl_thd_param_set(low, spl.v);
	sl_thd_param_set(high, sph.v);
}

#define TEST_ITERS 1000

void
test_high_wakeup(void *data)
{
	unsigned int   toggle = 0, iters = 0;
	struct sl_thd *t     = data;
	cycles_t       start = sl_now();

	while (1) {
		cycles_t timeout = sl_now() + sl_usec2cyc(100);

		if (toggle % 10 == 0)
			printc(".h:%llums.", sl_cyc2usec(sl_thd_block_timeout(0, timeout)));
		else
			printc(".h:%up.", sl_thd_block_periodic(0));

		toggle++;
		iters++;

		if (iters == TEST_ITERS) {
			printc("\nTest done! (Duration: %llu ms)\n", sl_cyc2usec(sl_now() - start) / 1000);
			printc("Deleting all threads. Idle thread should take over!\n");
			sl_thd_free(t);
			sl_thd_free(sl_thd_curr());

			/* should not be scheduled. */
			assert(0);
		}
	}
}

void
test_timeout_wakeup(void)
{
	struct sl_thd *   low, *high;
	union sched_param sph = { .c = { .type = SCHEDP_PRIO, .value = 5 } };
	union sched_param spl = { .c = { .type = SCHEDP_PRIO, .value = 10 } };
	union sched_param spw = { .c = { .type = SCHEDP_WINDOW, .value = 1000 } };

	low = sl_thd_alloc(test_low, NULL);
	sl_thd_param_set(low, spl.v);
	sl_thd_param_set(low, spw.v);

	high = sl_thd_alloc(test_high_wakeup, low);
	sl_thd_param_set(high, sph.v);
	sl_thd_param_set(high, spw.v);
}

void
cos_init(void)
{
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo *   ci    = cos_compinfo_get(defci);

	printc("Unit-test for the scheduling library (sl)\n");
	cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_defcompinfo_init();
	sl_init();

	//	test_yields();
	//	test_blocking_directed_yield();
	test_timeout_wakeup();

	sl_sched_loop();

	assert(0);

	return;
}

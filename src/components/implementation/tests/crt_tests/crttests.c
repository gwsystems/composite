/*
 * Copyright 2016, Phani Gadepalli and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <cos_component.h>
#include <cos_defkernel_api.h>
#include <llprint.h>
#include <sl.h>

#include <crt_lock.h>

#define LOCK_ITER 1000000
#define NTHDS 4
struct crt_lock lock;
struct sl_thd  *lock_thds[NTHDS] = {NULL, };
unsigned int    progress[NTHDS] = {0, };
struct cos_compinfo *ci;

thdid_t
next_thd(void)
{
	return sl_thd_thdid(lock_thds[(unsigned int)(ps_tsc() % NTHDS)]);
}

volatile thdid_t holder;

void
lock_thd(void *d)
{
	int i, cnt, me = -1;

	for (i = 0; i < NTHDS; i++) {
		if (sl_thd_thdid(lock_thds[i]) != cos_thdid()) continue;

		me = i;
	}
	assert(me != -1);

	sl_thd_yield(sl_thd_thdid(lock_thds[1]));

	for (i = 0; i < LOCK_ITER; i++) {
		crt_lock_take(&lock);

		progress[me]++;
		holder = cos_thdid();

		sl_thd_yield(next_thd());

		if (holder != cos_thdid()) {
			printc("FAILURE\n");
			BUG();
		}
		crt_lock_release(&lock);
		sl_thd_yield(next_thd());
	}

	for (i = 0; i < NTHDS; i++) {
		if (i == me) continue;

		if (progress[i] < LOCK_ITER) {
			sl_thd_yield(sl_thd_thdid(lock_thds[i]));
		}
	}

	printc("SUCCESS!");
	while (1) ;
}

void
test_lock(void)
{
	int i;
	union sched_param_union sps[] = {
		{.c = {.type = SCHEDP_PRIO, .value = 5}},
		{.c = {.type = SCHEDP_PRIO, .value = 6}},
		{.c = {.type = SCHEDP_PRIO, .value = 6}},
		{.c = {.type = SCHEDP_PRIO, .value = 7}}
	};

	crt_lock_init(&lock);

	printc("Create threads:\n");
	for (i = 0; i < NTHDS; i++) {
		lock_thds[i] = sl_thd_alloc(lock_thd, NULL);
		printc("\tcreating thread %d at prio %d\n", sl_thd_thdid(lock_thds[i]), sps[i].c.value);
		sl_thd_param_set(lock_thds[i], sps[i].v);
	}
}

void
cos_init(void)
{
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	ci = cos_compinfo_get(defci);

	printc("Unit-test for the crt (sl)\n");
	cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_defcompinfo_init();
	sl_init(SL_MIN_PERIOD_US);

	test_lock();

	printc("Running benchmark...\n");
	sl_sched_loop_nonblock();

	assert(0);

	return;
}

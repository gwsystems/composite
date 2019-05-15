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
#include <crt_chan.h>

struct cos_compinfo *ci;

#define CHAN_ITER  1000000
#define NCHANTHDS  5
#define CHAN_BATCH 3

CRT_CHAN_STATIC_ALLOC(c0, int, 4);
CRT_CHAN_STATIC_ALLOC(c1, int, 4);
CRT_CHAN_STATIC_ALLOC(c2, int, 4);
CRT_CHAN_STATIC_ALLOC(c3, int, 4);
CRT_CHAN_STATIC_ALLOC(c4, int, 4);

CRT_CHAN_TYPE_PROTOTYPES(test, int, 4);
struct crt_chan *chans[NCHANTHDS + 1];
struct sl_thd  *chan_thds[NCHANTHDS] = {NULL, };

typedef enum { CHILLING = 0, RECVING, SENDING } actions_t;
unsigned long status[NCHANTHDS];
unsigned long cnts[NCHANTHDS] = {0, };

int
chantest_is_deadlocked(void)
{
	int i;
	actions_t s = status[0];

	/* Are all threads in the same blocked state? */
	for (i = 0; i < NCHANTHDS; i++) {
		if (status[i] == CHILLING || status[i] != s) return 0;
	}

	return 1;
}

void
chantest_send(int thd_off, struct crt_chan *c)
{
	int send = cos_thdid();

	if (crt_chan_full_test(c)) status[thd_off] = SENDING;
	if (!chantest_is_deadlocked()) {
		/* printc("\t%d: send\n", cos_thdid()); */
		crt_chan_send_test(c, &send);
	}
	status[thd_off] = CHILLING;
}

void
chantest_recv(int thd_off, struct crt_chan *c)
{
	int recv;

	if (crt_chan_empty_test(c)) status[thd_off] = RECVING;
	if (!chantest_is_deadlocked()) {
		/* printc("\t%d: recv\n", cos_thdid()); */
		crt_chan_recv_test(c, &recv);
		cnts[thd_off]++;
	}
	status[thd_off] = CHILLING;
}

void
chan_thd(void *d)
{
	int thd_off = (int)d;
	struct crt_chan **chan_pair = &chans[thd_off];
	int recv;
	int i;

	for (i = 0; i < CHAN_ITER; i++) {
		int j;

		/* printc("%d: pre-send\n", cos_thdid()); */
		for (j = 0; j < CHAN_BATCH; j++) {
			chantest_send(thd_off, chan_pair[1]);
		}

		/* printc("%d: pre-recv\n", cos_thdid()); */
		for (j = 0; j < CHAN_BATCH; j++) {
			chantest_recv(thd_off, chan_pair[0]);
		}
	}

	printc("SUCCESS! Counts (should be within %d of each other): ", NCHANTHDS * CHAN_BATCH);
	for (i = 0; i < NCHANTHDS; i++) {
		printc("\t%ld", cnts[i]);
	}
	printc("\n");
	while (1) ;
}

void
idle_thd(void *d)
{
	printc("FAILURE: deadlock!\n");
	while (1) ;
}

void
test_chan(void)
{
	int i;
	struct sl_thd *idle;
	union sched_param_union idle_param = {.c = {.type = SCHEDP_PRIO, .value = 10}};

	union sched_param_union sps[] = {
		{.c = {.type = SCHEDP_PRIO, .value = 7}},
		{.c = {.type = SCHEDP_PRIO, .value = 6}},
		{.c = {.type = SCHEDP_PRIO, .value = 8}},
		{.c = {.type = SCHEDP_PRIO, .value = 5}},
		{.c = {.type = SCHEDP_PRIO, .value = 5}}
	};

	chans[0] = c0;
	chans[1] = c1;
	chans[2] = c2;
	chans[3] = c3;
	chans[4] = c4;
	chans[5] = c0;

	for (i = 0; i < NCHANTHDS; i++) {
		crt_chan_init_test(chans[i]);
	}

	printc("Create threads:\n");
	for (i = 0; i < NCHANTHDS; i++) {
		chan_thds[i] = sl_thd_alloc(chan_thd, (void *)i);
		assert(chan_thds[i]);
		printc("\tcreating thread %d at prio %d\n", sl_thd_thdid(chan_thds[i]), sps[i].c.value);
		sl_thd_param_set(chan_thds[i], sps[i].v);
	}
	idle = sl_thd_alloc(idle_thd, NULL);
	printc("\tcreating IDLE %d at prio %d\n", sl_thd_thdid(idle), idle_param.c.value);
	sl_thd_param_set(idle, idle_param.v);

}

#define LOCK_ITER 1000000
#define NLOCKTHDS 4
struct crt_lock lock;
struct sl_thd  *lock_thds[NLOCKTHDS] = {NULL, };
unsigned int    progress[NLOCKTHDS] = {0, };
volatile thdid_t holder;

thdid_t
next_thd(void)
{
	return sl_thd_thdid(lock_thds[(unsigned int)(ps_tsc() % NLOCKTHDS)]);
}

void
lock_thd(void *d)
{
	int i, cnt, me = -1;

	for (i = 0; i < NLOCKTHDS; i++) {
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

	for (i = 0; i < NLOCKTHDS; i++) {
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
	for (i = 0; i < NLOCKTHDS; i++) {
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
//	test_chan();

	printc("Running benchmark...\n");
	sl_sched_loop_nonblock();

	assert(0);

	return;
}

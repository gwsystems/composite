/*
 * Copyright 2016, Phani Gadepalli, Runyu Pan and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 * Care should be taken when running this: may not finish on an embedded board.
 * To finish, reduce iterations to 100.
 */

#include <llprint.h>
#include <sched.h>

#include <crt_lock.h>
#include <crt_sem.h>
#include <crt_static_chan.h>
#include <cos_time.h>

#define CHAN_ITER  10000
#define NCHANTHDS  5
#define CHAN_BATCH 3

#define SWITCH_TO sched_thd_yield_to

CRT_STATIC_CHAN_STATIC_ALLOC(c0, int, 4);
CRT_STATIC_CHAN_STATIC_ALLOC(c1, int, 4);
CRT_STATIC_CHAN_STATIC_ALLOC(c2, int, 4);
CRT_STATIC_CHAN_STATIC_ALLOC(c3, int, 4);
CRT_STATIC_CHAN_STATIC_ALLOC(c4, int, 4);

CRT_STATIC_CHAN_TYPE_PROTOTYPES(test, int, 4);
struct crt_static_chan *chans[NCHANTHDS + 1];
thdid_t chan_thds[NCHANTHDS] = {0, };

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
chantest_send(int thd_off, struct crt_static_chan *c)
{
	int send = cos_thdid();

	if (crt_static_chan_full_test(c)) status[thd_off] = SENDING;
	if (!chantest_is_deadlocked()) {
		/* printc("\t%d: send\n", cos_thdid()); */
		crt_static_chan_send_test(c, &send);
	}
	status[thd_off] = CHILLING;
}

void
chantest_recv(int thd_off, struct crt_static_chan *c)
{
	int recv;

	if (crt_static_chan_empty_test(c)) status[thd_off] = RECVING;
	if (!chantest_is_deadlocked()) {
		/* printc("\t%d: recv\n", cos_thdid()); */
		crt_static_chan_recv_test(c, &recv);
		cnts[thd_off]++;
	}
	status[thd_off] = CHILLING;
}

void
chan_thd(void *d)
{
	int thd_off = (unsigned long)d;
	struct crt_static_chan **chan_pair = &chans[thd_off];
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
	unsigned long i;
	thdid_t idle;
	sched_param_t idle_param = SCHED_PARAM_CONS(SCHEDP_PRIO, 10);

	sched_param_t sps[] = {
		SCHED_PARAM_CONS(SCHEDP_PRIO, 7),
		SCHED_PARAM_CONS(SCHEDP_PRIO, 6),
		SCHED_PARAM_CONS(SCHEDP_PRIO, 8),
		SCHED_PARAM_CONS(SCHEDP_PRIO, 5),
		SCHED_PARAM_CONS(SCHEDP_PRIO, 5)
	};
	unsigned int p;

	chans[0] = c0;
	chans[1] = c1;
	chans[2] = c2;
	chans[3] = c3;
	chans[4] = c4;
	chans[5] = c0;

	for (i = 0; i < NCHANTHDS; i++) {
		crt_static_chan_init_test(chans[i]);
	}

	printc("Create threads:\n");
	for (i = 0; i < NCHANTHDS; i++) {
		chan_thds[i] = sched_thd_create(chan_thd, (void *)i);
		assert(chan_thds[i]);
		sched_param_get(sps[i], NULL, &p);
		printc("\tcreating thread %lu at prio %d\n", chan_thds[i], p);
		sched_thd_param_set(chan_thds[i], sps[i]);
	}
	idle = sched_thd_create(idle_thd, NULL);
	sched_param_get(idle_param, NULL, &p);
	printc("\tcreating IDLE %lu at prio %d\n", idle, p);
	sched_thd_param_set(idle, idle_param);

}

#define LOCK_ITER 10000
#define NLOCKTHDS 4
struct crt_lock lock;
thdid_t lock_thds[NLOCKTHDS] = {0, };
unsigned int lock_progress[NLOCKTHDS] = {0, };
volatile thdid_t holder;

thdid_t
next_lock_thd(void)
{
	return lock_thds[(unsigned int)(time_now() % NLOCKTHDS)];
}

void
lock_thd(void *d)
{
	int i, cnt, me = -1;

	for (i = 0; i < NLOCKTHDS; i++) {
		if (lock_thds[i] != cos_thdid()) continue;

		me = i;
	}
	assert(me != -1);

	SWITCH_TO(lock_thds[1]);

	for (i = 0; i < LOCK_ITER; i++) {
		crt_lock_take(&lock);

		lock_progress[me]++;
		holder = cos_thdid();

		SWITCH_TO(next_lock_thd());

		if (holder != cos_thdid()) {
			printc("FAILURE\n");
			BUG();
		}
		crt_lock_release(&lock);
		SWITCH_TO(next_lock_thd());
	}

	for (i = 0; i < NLOCKTHDS; i++) {
		if (i == me) continue;

		if (lock_progress[i] < LOCK_ITER) {
			SWITCH_TO(lock_thds[i]);
		}
	}

	printc("SUCCESS!");
	while (1) ;
}

void
test_lock(void)
{
	int i;
	sched_param_t sps[] = {
		SCHED_PARAM_CONS(SCHEDP_PRIO, 6),
		SCHED_PARAM_CONS(SCHEDP_PRIO, 6),
		SCHED_PARAM_CONS(SCHEDP_PRIO, 6),
		SCHED_PARAM_CONS(SCHEDP_PRIO, 6)
	};

	crt_lock_init(&lock);

	printc("Create threads:\n");
	for (i = 0; i < NLOCKTHDS; i++) {
		lock_thds[i] = sched_thd_create(lock_thd, NULL);
		printc("\tcreating thread %lu at prio %d\n", lock_thds[i], sps[i]);
		sched_thd_param_set(lock_thds[i], sps[i]);
	}
}

#define SEM_ITER 10000
#define NSEMTHDS 4
struct crt_sem sem;
thdid_t sem_thds[NSEMTHDS] = {0, };
unsigned int sem_progress[NSEMTHDS] = {0, };
volatile thdid_t poster;

thdid_t
next_sem_thd(void)
{
	return sem_thds[(unsigned int)(time_now() % NSEMTHDS)];
}

void
sem_thd(void *d)
{
	int i, cnt, me = -1;

	for (i = 0; i < NSEMTHDS; i++) {
		if (sem_thds[i] != cos_thdid()) continue;

		me = i;
	}
	assert(me != -1);

	SWITCH_TO(sem_thds[1]);

	for (i = 0; i < SEM_ITER; i++) {
		crt_sem_take(&sem);

		sem_progress[me]++;
		poster = cos_thdid();

		SWITCH_TO(next_sem_thd());

		if (poster != cos_thdid()) {
			printc("FAILURE\n");
			BUG();
		}
		crt_sem_give(&sem);
		SWITCH_TO(next_sem_thd());
	}

	for (i = 0; i < NSEMTHDS; i++) {
		if (i == me) continue;

		if (sem_progress[i] < SEM_ITER) {
			SWITCH_TO(sem_thds[i]);
		}
	}

	printc("SUCCESS!");
	while (1) ;
}

void
test_sem(void)
{
	int i;
	sched_param_t sps[] = {
		SCHED_PARAM_CONS(SCHEDP_PRIO, 6),
		SCHED_PARAM_CONS(SCHEDP_PRIO, 6),
		SCHED_PARAM_CONS(SCHEDP_PRIO, 6),
		SCHED_PARAM_CONS(SCHEDP_PRIO, 6)
	};

	crt_sem_init(&sem, 1);

	printc("Create threads:\n");
	for (i = 0; i < NSEMTHDS; i++) {
		sem_thds[i] = sched_thd_create(sem_thd, NULL);
		printc("\tcreating thread %lu at prio %d\n", sem_thds[i], sps[i]);
		sched_thd_param_set(sem_thds[i], sps[i]);
	}
}

void
cos_init(void)
{
	printc("Unit-test for the crt (w/sched interface).\n");
}

int
main(void)
{
	/* Run the uncommented test - one at a time */
//	test_lock();
//	test_sem();
	test_chan();

	printc("Running benchmark, exiting main thread...\n");

	return 0;
}

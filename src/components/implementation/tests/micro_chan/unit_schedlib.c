/*
 * Copyright 2016, Phani Gadepalli and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <cos_component.h>
#include <cobj_format.h>
#include <cos_defkernel_api.h>
#include <llprint.h>
#include <sl.h>
#include <cos_dcb.h>
#include <crt_chan.h>
#include <crt_lock.h>

/* Iterations, channels */
#define CHAN_ITER  1000000
#define NCHANTHDS  2
#define CHAN_BATCH 3

unsigned long long iters[CHAN_ITER] = { 0 };

CRT_CHAN_STATIC_ALLOC(c0, int, 4);
CRT_CHAN_TYPE_PROTOTYPES(test, int, 4);
struct crt_lock lock;

unsigned int one_only = 0;

typedef enum { CHILLING = 0, RECVING, SENDING } actions_t;
unsigned long status[NCHANTHDS];
unsigned long cnts[NCHANTHDS] = {0, };

/* sl also defines a SPIN macro */
#undef SPIN
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


#define N_TESTTHDS 2
#define WORKITERS 100

#define N_TESTTHDS_PERF 2
#define PERF_ITERS 1000

static volatile cycles_t mid_cycs = 0;
static volatile int testing = 1;

void
test_thd_perffn(void *data)
{
	cycles_t start_cycs = 0, end_cycs = 0, wc_cycs = 0, total_cycs = 0;
	unsigned int i = 0;

	rdtscll(start_cycs);
	sl_thd_yield(0);
	rdtscll(end_cycs);
	assert(mid_cycs && mid_cycs > start_cycs && mid_cycs < end_cycs);

	for (i = 0; i < PERF_ITERS; i++) {
		cycles_t diff1_cycs = 0, diff2_cycs = 0;

		mid_cycs = 0;
		rdtscll(start_cycs);
		sl_thd_yield(0);
		rdtscll(end_cycs);
		assert(mid_cycs && mid_cycs > start_cycs && mid_cycs < end_cycs);

		diff1_cycs = mid_cycs - start_cycs;
		diff2_cycs = end_cycs - mid_cycs;

		if (diff1_cycs > wc_cycs) wc_cycs = diff1_cycs;
		if (diff2_cycs > wc_cycs) wc_cycs = diff2_cycs;
		total_cycs += (diff1_cycs + diff2_cycs);
	}

	PRINTC("SWITCH UBENCH: avg: %llu, wc: %llu, iters:%u\n", (total_cycs / (2 * PERF_ITERS)), wc_cycs, PERF_ITERS);
	testing = 0;
	/* done testing! let the spinfn cleanup! */
	sl_thd_yield(0);

	sl_thd_exit();
}

void
test_thd_spinfn(void *data)
{
	while (likely(testing)) {
		rdtscll(mid_cycs);
		sl_thd_yield(0);
	}

	sl_thd_exit();
}
/* Get the numbers */
volatile unsigned long long start_time;
volatile unsigned long long end_time;
//void
//test_thd_fn(void *data)
//{
//	cycles_t time;
//	cycles_t iters;
//	int rounds = 0;
//	if (data!=0) {
//		while (1) {
//			rounds++;
//			rdtscll(start_time);
//			sl_thd_yield(3);
//			rdtscll(end_time);
//			print_uint((unsigned long)(end_time-start_time));
//			print_string("\r\n");
//			if(rounds == 10000)
//				while(1);
//		}
//	}
//	else {
//		while (1) {
//			sl_thd_yield(4);
//		}
//	}
//}

#define RCV 0
#define SND 1

void
test_thd_fn(void *data)
{
	cycles_t time;
//	cycles_t iters;
	cycles_t total = 0, max = 0, diff;
	int send;
	int recv;
	int rounds = 0;
	if (data==RCV) {
		while (1) {
			rounds ++;
			crt_chan_recv_test(c0, &recv);
			rdtscll(end_time);
			assert(ps_faa(&one_only, -1) == 1);

			diff = end_time - start_time;
			if (diff > max) max = diff;
			total += diff;
			iters[rounds - 1] = diff;
			//printc("%llu, ", diff);

			if (rounds == CHAN_ITER) {
				int i;

				for (i = 0; i < CHAN_ITER; i++) {
					printc("%llu\n", iters[i]);
				}
				printc("\nAvg: %llu, Wc:%llu\n", total / CHAN_ITER, max);

				while (1) ;
			}
			//print_uint((unsigned long)(end_time-start_time));
			//print_string("\r\n");
			//if(rounds == 10000)
			//	while(1);
		}
	}
	else {
		send = 0x1234;
		while (1) {
			assert(ps_faa(&one_only, 1) == 0);
			rdtscll(start_time);
			crt_chan_send_test(c0, &send);
		}
	}
}

//void
//test_thd_fn(void *data)
//{
//	cycles_t time;
//	cycles_t iters;
//	int send;
//	int recv;
//	int rounds = 0;
//
//	if (data!=0) {
//		while (1) {
//			rounds ++;
//
//			crt_lock_take(&lock);
//			sl_thd_yield(0);
//			rdtscll(end_time);
//			crt_lock_release(&lock);
//			sl_thd_yield(0);
//
//			print_uint((unsigned long)(end_time-start_time));
//			print_string("\r\n");
//			if(rounds == 10000)
//				while(1);
//		}
//	}
//	else {
//		crt_lock_init(&lock);
//		while (1) {
//			rdtscll(start_time);
//			crt_lock_take(&lock);
//			crt_lock_release(&lock);
//			sl_thd_yield(0);
//		}
//	}
//}
//
//volatile unsigned long long int_tsc;
//void
//test_thd_fn(capid_t cap, void *data)
//{
//	cycles_t time;
//	cycles_t iters;
//	int send;
//	int recv;
//	unsigned int result;
//	int rounds = 0;
//	if (data==0) {
//		while (1) {
//			//print_string("*");
//		}
//	}
//	else {
//		/* Higher priority on this branch */
//		cos_hw_attach(BOOT_CAPTBL_SELF_INITHW_BASE, 63, sl_thd_rcvcap(sl_thd_lkup(sl_thdid())));
//		cos_hw_custom(BOOT_CAPTBL_SELF_INITHW_BASE);
//		while (1) {
//			/* We are doing this receive anyway */
//			cos_rcv(sl_thd_rcvcap(sl_thd_lkup(sl_thdid())), 0);
//			rdtscll(end_time);
//			addr[rounds] = (unsigned int)(end_time-int_tsc);
//			rounds ++;
//			if(rounds == 10000)
//			{
//				for (rounds = 0; rounds < 10000; rounds ++)
//				{
//					print_uint(addr[rounds]);
//					print_string("\r\n");
//				}
//				while(1);
//			}
//		}
//	}
//}

//	int rounds = 0;
//void
//test_thd_fn(capid_t cap, void *data)
//{
//	cycles_t time;
//	cycles_t iters;
//	int send;
//	int recv;
//	unsigned int result;
//	/* if (data == 0) {
//		while (1) {
//			print_string("*");
//		}
//	}
//	else */if (data == 0)
//	{
//		/* Higher priority on this branch - receiving stuff from interrupt */
//		cos_hw_attach(BOOT_CAPTBL_SELF_INITHW_BASE, 63, sl_thd_rcvcap(sl_thd_lkup(sl_thdid())));
//		cos_hw_custom(BOOT_CAPTBL_SELF_INITHW_BASE);
//		while (1) {
////			print_string(" :1a: \r\n");
//			/* We are doing this receive anyway */
////			sl_thd_rcv(RCV_ULONLY);
//			cos_rcv(sl_thd_rcvcap(sl_thd_lkup(sl_thdid())), 0);
////			print_string(" :1b: ");
//			/* Send to the guy immediately */
//			crt_chan_send_test(c0, &send);
//			//sl_thd_wakeup(4);
////			print_string(" :1c: ");
//			//rdtscll(end_time);
//			//addr[rounds] = (unsigned int)(end_time-int_tsc);
//		}
//	}
//	else {
//		while(1) {
//			/* Finally, we send what we receive here */
////			print_string(" :2a: ");
//			//sl_thd_block(0);
//			crt_chan_recv_test(c0, &recv);
////			print_string(" :2b: ");
//			rdtscll(end_time);
//			//print_uint(addr[rounds]);
//			//print_string(" - ");
//			addr[rounds] = (unsigned int)(end_time-int_tsc);
//			//print_uint(addr[rounds]);
//			//print_string("\r\n");
//			rounds ++;
//			if(rounds == 10000)
//			{
//				for (rounds = 0; rounds < 10000; rounds ++)
//				{
//					print_uint(addr[rounds]);
//					print_string("\r\n");
//				}
//				while(1);
//			}
//		}
//	}
//}

//void
//test_yield_perf(void)
//{
//	int                     i;
//	struct sl_thd          *threads[N_TESTTHDS_PERF];
//	union sched_param_union sp = {.c = {.type = SCHEDP_PRIO, .value = 31}};
//
//	for (i = 0; i < N_TESTTHDS_PERF; i++) {
//		if (i == 1) threads[i] = sl_thd_alloc(test_thd_perffn, (void *)&threads[0]);
//		else        threads[i] = sl_thd_alloc(test_thd_spinfn, NULL);
//		assert(threads[i]);
//		sl_thd_param_set(threads[i], sp.v);
//		PRINTC("Thread %u:%lu created\n", sl_thd_thdid(threads[i]), sl_thd_thdcap(threads[i]));
//	}
//}

//void
//test_yields(void)
//{
//	int                     i;
//	struct sl_thd *         threads[N_TESTTHDS];
//	union sched_param_union sp = {.c = {.type = SCHEDP_PRIO, .value = 10}};
//
//	for (i = 0; i < N_TESTTHDS; i++) {
//		threads[i] = sl_thd_alloc(test_thd_fn, (void *)i);
//		assert(threads[i]);
//		sl_thd_param_set(threads[i], sp.v);
//		PRINTC("Thread %u:%lu created\n", sl_thd_thdid(threads[i]), sl_thd_thdcap(threads[i]));
//	}
//}

void
test_yields(void)
{
	int                     i;
	struct sl_thd *         threads[N_TESTTHDS];
	union sched_param_union sp = {.c = {.type = SCHEDP_PRIO, .value = 0}};

	start_time = end_time = 0;

	for (i = 0; i < N_TESTTHDS; i++) {
		threads[i] = sl_thd_alloc(test_thd_fn, (void *)i);
		assert(threads[i]);
		if (i == RCV) sp.c.value = 2;
		else          sp.c.value = 5;
		sl_thd_param_set(threads[i], sp.v);
		PRINTC("Thread %u:%lu created\n", sl_thd_thdid(threads[i]), sl_thd_thdcap(threads[i]));
		//sl_thd_yield_thd(threads[i]);
	}
	assert(N_TESTTHDS == 2);
	//crt_chan_p2p_init_test(c0, threads[SND], threads[RCV]);
	crt_chan_init_test(c0);
}

//void
//test_yields(void)
//{
//	int                     i;
//	struct sl_thd *         threads[N_TESTTHDS];
//	union sched_param_union sp = {.c = {.type = SCHEDP_PRIO, .value = 10}};
//
//	crt_chan_init_test(&c0);
//	for (i = 0; i < N_TESTTHDS; i++) {
//		threads[i] = sl_thd_aep_alloc(test_thd_fn, (void *)i, 0, 0, 0, 0);
//		assert(threads[i]);
//		if(i != 0)
//			sp.c.value = 9;
//		sl_thd_param_set(threads[i], sp.v);
//		PRINTC("Thread %u:%lu created\n", sl_thd_thdid(threads[i]), sl_thd_thdcap(threads[i]));
//	}
//}

void
test_high(void *data)
{
	struct sl_thd *t = data;

	while (1) {
		sl_thd_yield(sl_thd_thdid(t));
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
	struct sl_thd *         low, *high;
	union sched_param_union sph = {.c = {.type = SCHEDP_PRIO, .value = 5}};
	union sched_param_union spl = {.c = {.type = SCHEDP_PRIO, .value = 10}};

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
	struct sl_thd *         low, *high;
	union sched_param_union sph = {.c = {.type = SCHEDP_PRIO, .value = 5}};
	union sched_param_union spl = {.c = {.type = SCHEDP_PRIO, .value = 10}};
	union sched_param_union spw = {.c = {.type = SCHEDP_WINDOW, .value = 1000}};

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
	/* This is a hack, we know where the heap is */
	cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_defcompinfo_llinit();
	cos_dcb_info_init_curr();
	sl_init(SL_MIN_PERIOD_US*50);

	//test_yield_perf();
	test_yields();
	//test_blocking_directed_yield();
	//test_timeout_wakeup();

	sl_sched_loop_nonblock();

	assert(0);

	return;
}

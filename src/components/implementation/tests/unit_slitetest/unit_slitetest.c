/*
 * Copyright 2018, Phani Gadepalli and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <llprint.h>
#include <res_spec.h>
#include <sched.h>
#include <cos_time.h>
#include <perfdata.h>

#define SL_FPRR_NPRIOS 32

#define LOWEST_PRIORITY (SL_FPRR_NPRIOS - 1)

#define LOW_PRIORITY (LOWEST_PRIORITY - 1)
#define HIGH_PRIORITY (LOWEST_PRIORITY - 10)

#define TEST_RCV_CORE 0
#define TEST_SND_CORE 1

#define TEST_ITERS 20

static volatile int test_done = 0;
static volatile thdid_t thd[NUM_CPU] = { 0 };
static volatile thdid_t spin_thd[2] = { 0 };

static volatile unsigned long long total_rcvd[NUM_CPU] = { 0 };
static volatile unsigned long long total_sent[NUM_CPU] = { 0 };

static volatile cycles_t global_time[2] = { 0 };
static volatile cycles_t time = 0;

static struct perfdata pd;

#define ARRAY_SIZE 10000
static cycles_t results[ARRAY_SIZE];

static void
unit_rcv(thdid_t tid)
{
	int rcvd;

	rcvd = sched_arcv(tid);
	total_rcvd[cos_cpuid()] += rcvd;
	return;
}

static void
rcv_spiner()
{
	while (!spin_thd[1]) ;
	while (1) {
		printc("*************spiner1**************: %ld\n\n", spin_thd[0]);
		sched_thd_yield_to(spin_thd[1]);
		rdtscll(global_time[0]);
	}
	assert(0);
	SPIN();
	return;
}

static void
rcv_spiner2()
{
	while (!spin_thd[0]) ;
	while (1) {
		printc("*************spiner2**************: %ld\n\n", spin_thd[1]);
		sched_thd_yield_to(spin_thd[0]);
		rdtscll(global_time[0]);
	}
	return;
}

static void
unit_snd(thdid_t tid)
{
	int ret = 0;

	ret = sched_asnd(tid);
	assert(ret == 0 || ret == -EBUSY);

	if (!ret) total_sent[cos_cpuid()]++;
	return;
}

static void
test_snd_fn()
{
	int iters = 0;

	thdid_t s = thd[TEST_RCV_CORE];
	assert(s);
	thdid_t r = thd[TEST_SND_CORE];
	assert(r);
	perfdata_init(&pd, "TEST IPI Switch", results, ARRAY_SIZE);
	
	for (iters = 0; iters < TEST_ITERS; iters ++) {
		while (global_time[0] < global_time[1]) {};
		printc("snd->\n");
		unit_snd(s);
		printc("sent, now rcv\n");
		unit_rcv(r);
		printc("rcvd\n");
	}

	perfdata_calc(&pd);
	
//	printc("Test IPI INTERRUPT W Switch:\t AVG:%llu, MAX:%llu, MIN:%llu, ITER:%d\n",
//		    perfdata_avg(&pd), perfdata_max(&pd),
//            perfdata_min(&pd), perfdata_sz(&pd));

//	printc("\t\t\t\t\t SD:%llu, 90%%:%llu, 95%%:%llu, 99%%:%llu\n",
//            perfdata_sd(&pd),perfdata_90ptile(&pd),
//            perfdata_95ptile(&pd), perfdata_99ptile(&pd));

	test_done = 1;
	SPIN();
	assert(0);
}

static void
test_rcv_fn()
{
	thdid_t s = thd[TEST_SND_CORE];
	assert(s);
	thdid_t r = thd[TEST_RCV_CORE];
	assert(r);

	while (1) {
	printc("rcv\n");
		unit_rcv(r);
	printc("rcv done\n");
		rdtscll(global_time[1]);
		time = (global_time[1] - global_time[0]);
		perfdata_add(&pd, time);
		unit_snd(s);
	};
}

void
test_ipi_switch(void)
{
	asndcap_t snd = 0;

	if (cos_cpuid() == TEST_RCV_CORE) {
		printc("test ipi switch\n");
		thd[cos_cpuid()] = sched_thd_create(test_rcv_fn, NULL);
		sched_thd_param_set(thd[cos_cpuid()], sched_param_pack(SCHEDP_PRIO, HIGH_PRIORITY));

		spin_thd[0] = sched_thd_create(rcv_spiner, NULL);
		sched_thd_param_set(spin_thd[0], sched_param_pack(SCHEDP_PRIO, HIGH_PRIORITY));

		spin_thd[1] = sched_thd_create(rcv_spiner2, NULL);
		sched_thd_param_set(spin_thd[1], sched_param_pack(SCHEDP_PRIO, HIGH_PRIORITY));
		SPIN();
		printc("rcvthd: %ld, spinthd0: %ld, spinthd1: %ld\n", thd[cos_cpuid()], spin_thd[0], spin_thd[1]);

		sched_thd_yield_to(spin_thd[0]);
	} else {
		thd[cos_cpuid()] = sched_thd_create(test_snd_fn, NULL);
		sched_thd_param_set(thd[cos_cpuid()], sched_param_pack(SCHEDP_PRIO, HIGH_PRIORITY));
	}
	SPIN();
}

int
main(void)
{
	assert(0);

	return 0;
}

void
parallel_main(coreid_t cid, int init_core, int ncores)
{
	int i = 0;

	test_ipi_switch();
	//test_done[cos_cpuid()] = 1;

	//for (i = 0; i < NUM_CPU; i++) {
	//	while (!test_done[i]);
	//}
	SPIN();

	if (cos_cpuid() == 0) printc("slite IPI test done.\n");

	assert(0);
	return;
}

void
cos_init(void)
{
	if (sched_scb_mapping()) BUG();
}

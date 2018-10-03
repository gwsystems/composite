/*
 * Copyright 2018, Phani Gadepalli and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <cos_types.h>
#include <cos_defkernel_api.h>
#include <llprint.h>
#include <res_spec.h>
#include <sl.h>
#include <capmgr.h>
#include <hypercall.h>
#include "perfdata.h"
#include <simple_sl.h>

/* FIXME: these defines are just getting uglier..*/
#undef TEST_SIMPLE_SCHED
#undef TEST_IPI_BATCHING
/* enable only one of these */
#undef TEST_IPC
#undef TEST_IPC_RAW
#undef TEST_LATENCY
#define TEST_RATE

#ifdef TEST_IPC
static volatile struct perfdata pd[3];
#else
static volatile struct perfdata pd;
#endif

#define HI_PRIO TCAP_PRIO_MAX
#define LOW_PRIO (HI_PRIO + 1)

#define AEP_BUDGET_US 10000
#define AEP_PERIOD_US 10000

#define IPI_MIN_THRESH 300
#define IPI_TEST_ITERS 1000000
#define SCHED_PERIOD_US 100000 /* 100ms */

static volatile cycles_t last = 0, total = 0, wc = 0, pwc = 0;
static volatile unsigned long count = 0;
static volatile asndcap_t c0_cn_asnd[NUM_CPU] = { 0 }, cn_c0_asnd[NUM_CPU] = { 0 };
static volatile arcvcap_t c0_rcv[NUM_CPU] = { 0 }, cn_rcv[NUM_CPU] = { 0 };
static volatile int testing = 0;

#define LAT_C0 0
#define LAT_C1 1
/* enable only one of these */
#undef RCV_UB_TEST
#undef SND_UB_TEST
#undef RPC_UB_TEST
#define CN_SND_ONLY

static void
hiprio_c0_lat_fn(arcvcap_t r, void *d)
{
	asndcap_t snd = c0_cn_asnd[(int)d];

	assert(snd);

	while (1) {
		int pending = 0, rcvd = 0, ret = 0;
		cycles_t now;

		if (unlikely(testing == 0)) break;

		pending = cos_rcv(r, RCV_ALL_PENDING, &rcvd);
		assert(pending == 0 && rcvd == 1);
		rdtscll(now);

#ifdef RCV_UB_TEST
		if (now - last >= IPI_MIN_THRESH) {
			count ++;
			total += now - last;
			if (unlikely(count == IPI_TEST_ITERS)) {
				PRINTC("IPI LATENCY (min:%u), WC:%llu(p:%llu), AVG:%llu(total:%llu, iter:%lu)\n",
				       IPI_MIN_THRESH, wc, pwc, total / count, total, count);
				testing = 0;
			}
		}
		if (now - last > wc) {
			pwc = wc;
			wc = now - last;
		}
#endif

		ret = cos_asnd(snd, 0);
		assert(ret == 0);
		rdtscll(last);
	}

	sl_thd_exit();
}

static void
hiprio_cn_lat_fn(arcvcap_t r, void *d)
{
	asndcap_t snd = cn_c0_asnd[(int)d];
	cycles_t sndtot = 0, sndwc = 0, sndpwc = 0;
	cycles_t rpctot = 0, rpcwc = 0, rpcpwc = 0;
	unsigned long iters = 0;

	assert(snd);

	while (1) {
		cycles_t st, en, rpcen;
		int pending = 0, rcvd = 0, ret = 0;

		if (unlikely(testing == 0)) break;

		rdtscll(st);
		ret = cos_asnd(snd, 0);
		assert(ret == 0);
		rdtscll(en);

#ifdef SND_UB_TEST
		iters ++;
		sndtot += (en - st);
		if (en - st > sndwc) {
			sndpwc = sndwc;
			sndwc  = en - st;
		}

		if (unlikely(iters == IPI_TEST_ITERS)) {
			PRINTC("SND WC:%llu(p:%llu), AVG:%llu(total:%llu, iter:%lu)\n",
			       sndwc, sndpwc, sndtot / iters, sndtot, iters);
			testing = 0;
		}
#endif

#ifndef CN_SND_ONLY
		pending = cos_rcv(r, RCV_ALL_PENDING, &rcvd);
		assert(pending == 0 && rcvd == 1);
		rdtscll(rpcen);
#ifdef RPC_UB_TEST
		iters ++;
		rpctot += (rpcen - st);
		if (rpcen - st > rpcwc) {
			rpcpwc = rpcwc;
			rpcwc  = rpcen - st;
		}

		if (unlikely(iters == IPI_TEST_ITERS)) {
			PRINTC("RPC WC:%llu(p:%llu), AVG:%llu(total:%llu, iter:%lu)\n",
			       rpcwc, rpcpwc, rpctot / iters, rpctot, iters);
			testing = 0;
		}
#endif
#endif
	}

	sl_thd_exit();
}

static void
loprio_c0_lat_fn(arcvcap_t r, void *d)
{
	while (1) {
		if (unlikely(testing == 0)) break;

		rdtscll(last);
	}

	sl_thd_exit();
}

static void
test_latency_setup(void)
{
#ifdef TEST_LATENCY
	static volatile int cdone[NUM_CPU] = { 0 };
	int i, ret;

	assert(NUM_CPU == 2);
	assert(LAT_C0 >= 0 && LAT_C1 >= 0 && LAT_C0 < NUM_CPU && LAT_C1 < NUM_CPU && LAT_C0 != LAT_C1);


	if (cos_cpuid() == LAT_C0) {
		asndcap_t snd = 0;
		struct sl_thd *lo = NULL, *hi = NULL;

		hi = sl_thd_aep_alloc(hiprio_c0_lat_fn, (void *)LAT_C1, 1, 0, 0, 0);
		assert(hi);
		c0_rcv[LAT_C1] = sl_thd_rcvcap(hi);

		lo = sl_thd_aep_alloc(loprio_c0_lat_fn, NULL, 1, 0, 0, 0);
		assert(lo);

		while (!cn_rcv[LAT_C1]) ;

		snd = capmgr_asnd_rcv_create(cn_rcv[LAT_C1]);
		assert(snd);
		c0_cn_asnd[LAT_C1] = snd;

		ret = cos_tcap_transfer(sl_thd_rcvcap(lo), BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, LOW_PRIO);
		assert(ret == 0);
		ret = cos_tcap_transfer(sl_thd_rcvcap(hi), BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, HI_PRIO);
		assert(ret == 0);

		sl_thd_param_set(hi, sched_param_pack(SCHEDP_WINDOW, AEP_PERIOD_US));
		sl_thd_param_set(hi, sched_param_pack(SCHEDP_BUDGET, AEP_BUDGET_US));
		sl_thd_param_set(hi, sched_param_pack(SCHEDP_PRIO, HI_PRIO));
		sl_thd_param_set(lo, sched_param_pack(SCHEDP_WINDOW, AEP_PERIOD_US));
		sl_thd_param_set(lo, sched_param_pack(SCHEDP_BUDGET, AEP_BUDGET_US));
		sl_thd_param_set(lo, sched_param_pack(SCHEDP_PRIO, LOW_PRIO));
		rdtscll(last);
		testing = 1;
	} else {
		struct sl_thd *hi = NULL;
		asndcap_t snd = 0;

		hi = sl_thd_aep_alloc(hiprio_cn_lat_fn, (void *)LAT_C1, 1, 0, 0, 0);
		assert(hi);
		cn_rcv[LAT_C1] = sl_thd_rcvcap(hi);

		while (!c0_rcv[LAT_C1]) ;

		snd = capmgr_asnd_rcv_create(c0_rcv[LAT_C1]);
		assert(snd);
		cn_c0_asnd[LAT_C1] = snd;

		ret = cos_tcap_transfer(sl_thd_rcvcap(hi), BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, HI_PRIO);
		assert(ret == 0);
		sl_thd_param_set(hi, sched_param_pack(SCHEDP_WINDOW, AEP_PERIOD_US));
		sl_thd_param_set(hi, sched_param_pack(SCHEDP_BUDGET, AEP_BUDGET_US));
		sl_thd_param_set(hi, sched_param_pack(SCHEDP_PRIO, HI_PRIO));
	}

	ps_faa((unsigned long *)&cdone[cos_cpuid()], 1);
	for (i = 0; i < NUM_CPU; i++) {
		while (!ps_load((unsigned long *)&cdone[i])) ;
	}
#endif
}

#define MEASURE_DL
#ifndef MEASURE_DL
#define WINDOW_US 1000
#define PRINT_US  1000000

#define WINDOW_SZ (PRINT_US/WINDOW_US)
volatile unsigned long ipi_rate[WINDOW_SZ] = { 0 }, ipi_winidx = 0;
volatile cycles_t ipi_win_st = 0, ipi_print_st = 0;

static void
hiprio_rate_c0_fn(arcvcap_t r, void *d)
{
	cycles_t now;
	unsigned long niters = 0;

	rdtscll(now);
	ipi_win_st = ipi_print_st = last = now;
	testing = 1;

	while (1) {
		cycles_t n2;

		rdtscll(now);

		if (now - last > wc) {
			pwc = wc;
			wc = now - last;
		}

		if (now - last >= IPI_MIN_THRESH) ipi_rate[ipi_winidx]++;

		if (now - ipi_win_st >= sl_usec2cyc(WINDOW_US)) {
			if (ipi_winidx < WINDOW_SZ - 1) {
				ipi_winidx++;
				ipi_rate[ipi_winidx] = 0;
			}
			ipi_win_st = now;
		}

		if (unlikely(now - ipi_print_st >= sl_usec2cyc(PRINT_US))) {
			unsigned long i, tot = 0;

			PRINTC("Rate: (win:%uus, period:%uus, wc:%llu pwc:%llu): ", WINDOW_US, PRINT_US, wc, pwc);
			for (i = 0; i < ipi_winidx; i++) {
				tot += ipi_rate[i];
				printc("%lu, ", ipi_rate[i]);
			}
			printc("[%lu]\n", tot);
			ipi_winidx = 0;
			ipi_rate[ipi_winidx] = 0;
			niters++;
			rdtscll(now);
			ipi_win_st = ipi_print_st = now;

			if (unlikely(niters >= IPI_TEST_ITERS)) {
				testing = 0;
				break;
			}
		}
		last = now;
	}

	sl_thd_exit();
}
#else

#define RATE_C0 0
/*
 * NOTE: Run spinlib in composite on baremetal with no timers, etc (micro_booter level),
 * get the number of iterations to spin for a given number of micro-seconds..
 * use that value here.. do the same for all envs (linux, cos, fiasco, and seL4).
 * and further adjust to make it more accurate.
 *
 * NOTE: I've tested ITERS_10US with multiples 2, 3 and 4 and the amount of spin
 * is 20us, 30us and 40us respectively without interference.
 */
#define ITERS_10US 5850
#define MULTIPLE   2
#undef NOINTERFERENCE_TEST

#define SPIN_ITERS (ITERS_10US*MULTIPLE) //note
#ifdef TEST_IPC_RAW
#define NITERS     1
#else
#define NITERS     1000
#endif

volatile unsigned int niters = 0;

static void __spin_fn(void) __attribute__ ((optimize("O0")));

static void
__spin_fn(void)
{
	unsigned int spin = 0;

	while (spin < SPIN_ITERS) {
		__asm__ __volatile__("nop": : :"memory");
		spin++;
	}
}

static void
hiprio_rate_c0_fn(arcvcap_t r, void *d)
{
	perfdata_init(&pd, "DLs");
	PRINTC("HI!\n");
	testing = 1;
	while (niters < NITERS) {
		cycles_t st = 0, en = 0;

		rdtscll(st);
		__spin_fn();
		rdtscll(en);

		perfdata_add(&pd, en - st);
		niters++;
	}

	testing = 0;
	perfdata_calc(&pd);
	perfdata_print(&pd);

	sl_thd_exit();
}

#endif

static void
loprio_rate_c0_fn(arcvcap_t r, void *d)
{
	asndcap_t snd = c0_cn_asnd[(int)d];

	assert(snd);
	while (testing == 0) ;

	while (1) {
		int pending = 0, rcvd = 0, ret = 0;

		if (unlikely(testing == 0)) break;

		pending = cos_rcv(r, RCV_ALL_PENDING, &rcvd);
		assert(pending == 0 && rcvd == 1);

#ifndef CN_SND_ONLY
		ret = cos_asnd(snd, 0);
		assert(ret == 0);
#endif
	}

	sl_thd_exit();
}

static void
hiprio_rate_cn_fn(arcvcap_t r, void *d)
{
	asndcap_t snd = cn_c0_asnd[(int)d];

	assert(snd);
	while (testing == 0) ;

	while (1) {
		int pending = 0, rcvd = 0, ret = 0;

		if (unlikely(testing == 0)) break;

	//	do {
			ret = cos_asnd(snd, 0);
	//	} while (unlikely(ret == -EBUSY));
	//	assert(ret == 0);

#ifndef CN_SND_ONLY
		pending = cos_rcv(r, RCV_ALL_PENDING, &rcvd);
		assert(pending == 0 && rcvd == 1);
#endif
	}

	sl_thd_exit();
}

static void
test_rate_setup(void)
{
#ifdef TEST_RATE
	static volatile int cdone[NUM_CPU] = { 0 };
	int i, ret;

	if (cos_cpuid() == RATE_C0) {
		struct sl_thd *lo[NUM_CPU] = { NULL }, *hi = NULL;
		asndcap_t snd = 0;

		hi = sl_thd_aep_alloc(hiprio_rate_c0_fn, NULL, 1, 0, 0, 0);
		assert(hi);

#ifndef NOINTERFERENCE_TEST
		for (i = 1; i < NUM_CPU; i++) {
			assert(i != RATE_C0);

			lo[i] = sl_thd_aep_alloc(loprio_rate_c0_fn, (void *)i, 1, 0, 0, 0);
			assert(lo[i]);
			c0_rcv[i] = sl_thd_rcvcap(lo[i]);

			while (!cn_rcv[i]) ;

#ifdef TEST_IPC_RAW
			snd = capmgr_asnd_rcv_create_raw(cn_rcv[i]);
#else
			snd = capmgr_asnd_rcv_create(cn_rcv[i]);
#endif
			assert(snd);
			c0_cn_asnd[i] = snd;
			ret = cos_tcap_transfer(sl_thd_rcvcap(lo[i]), BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, LOW_PRIO);
			assert(ret == 0);
			sl_thd_param_set(lo[i], sched_param_pack(SCHEDP_WINDOW, AEP_PERIOD_US));
			sl_thd_param_set(lo[i], sched_param_pack(SCHEDP_BUDGET, AEP_BUDGET_US));
			sl_thd_param_set(lo[i], sched_param_pack(SCHEDP_PRIO, LOW_PRIO));
		}
#endif
		ret = cos_tcap_transfer(sl_thd_rcvcap(hi), BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, HI_PRIO);
		assert(ret == 0);
		sl_thd_param_set(hi, sched_param_pack(SCHEDP_WINDOW, AEP_PERIOD_US));
		sl_thd_param_set(hi, sched_param_pack(SCHEDP_BUDGET, AEP_BUDGET_US));
		sl_thd_param_set(hi, sched_param_pack(SCHEDP_PRIO, HI_PRIO));
	} else {
#ifndef NOINTERFERENCE_TEST
		struct sl_thd *hi = NULL;
		asndcap_t snd = 0;

		hi = sl_thd_aep_alloc(hiprio_rate_cn_fn, (void *)cos_cpuid(), 1, 0, 0, 0);
		assert(hi);
		cn_rcv[cos_cpuid()] = sl_thd_rcvcap(hi);

		while (!c0_rcv[cos_cpuid()]) ;

#ifdef TEST_IPC_RAW
		snd = capmgr_asnd_rcv_create_raw(c0_rcv[cos_cpuid()]);
#else
		snd = capmgr_asnd_rcv_create(c0_rcv[cos_cpuid()]);
#endif
		assert(snd);
		cn_c0_asnd[cos_cpuid()] = snd;

		ret = cos_tcap_transfer(sl_thd_rcvcap(hi), BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, HI_PRIO);
		assert(ret == 0);
		sl_thd_param_set(hi, sched_param_pack(SCHEDP_WINDOW, AEP_PERIOD_US));
		sl_thd_param_set(hi, sched_param_pack(SCHEDP_BUDGET, AEP_BUDGET_US));
		sl_thd_param_set(hi, sched_param_pack(SCHEDP_PRIO, HI_PRIO));
#endif
	}

	ps_faa((unsigned long *)&cdone[cos_cpuid()], 1);
	for (i = 0; i < NUM_CPU; i++) {
		while (!ps_load((unsigned long *)&cdone[i])) ;
	}
#endif
}

volatile cycles_t c0_start = 0, c0_end = 0, c0_mid = 0, c1_start = 0, c1_end = 0, c1_mid = 0;

#define TEST_IPC_ITERS 1000000

static void
print_ipi_info(void)
{
	int i = 0;

	printc("====================================\n");
	printc("IPI Info:\n");
	for (i = 0; i < NUM_CPU; i ++) {
		unsigned int k_snd = 0, k_rcv = 0, c_snd = 0, c_rcv = 0;

		capmgr_core_ipi_counters_get(i, 0, &k_snd, &k_rcv);
		capmgr_core_ipi_counters_get(i, 1, &c_snd, &c_rcv);

		printc("CPU%d = %lu sent / %lu rcvd (kernel: %lu/%lu)\n", i, c_snd, c_rcv, k_snd, k_rcv);
	}
	printc("====================================\n");
}

static void
c0_ipc_fn(arcvcap_t r, void *d)
{
	asndcap_t snd = c0_cn_asnd[cos_cpuid()];
	int iters;
	cycles_t rtt_total = 0, one_total = 0, rtt_wc = 0, one_wc = 0, rone_total = 0, rone_wc = 0;

	assert(snd);
	rdtscll(c0_start);
#ifdef TEST_IPC
	perfdata_init((struct perfdata *)&pd[0], "RPC(0<->1)");
	perfdata_init((struct perfdata *)&pd[1], "IPC(0->1)");
//	perfdata_init((struct perfdata *)&pd[0], "IPC(0<-1)");
#endif
	c0_end = c0_mid = c1_start = c1_mid = c1_end = c0_start;

	testing = 1;
#ifdef TEST_IPI_BATCHING
	cos_rcv(r, 0, NULL);
#endif

	while (1) {
		int pending = 0, rcvd = 0, ret = 0;
		cycles_t rtt_diff, one_diff = 0, rone_diff = 0;

		rdtscll(c0_start);
		ret = cos_asnd(snd, 0);
		assert(ret == 0);

#ifndef TEST_IPI_BATCHING
		rdtscll(c0_mid);
		pending = cos_rcv(r, RCV_ALL_PENDING, &rcvd);
		assert(pending == 0 && rcvd == 1);
		rdtscll(c0_end);
#endif

		rtt_diff = (c0_end - c0_start);
		one_diff = (c1_mid - c0_start);
		rone_diff = (c0_end - c1_mid);
#ifdef TEST_IPC
#ifndef TEST_IPI_BATCHING
		perfdata_add(&pd[0], rtt_diff);
		perfdata_add(&pd[1], one_diff);
//		perfdata_add(&pd[2], rone_diff);
#endif
#endif
		//if (rtt_diff > rtt_wc) rtt_wc = rtt_diff;
		//if (one_diff > one_wc) one_wc = one_diff;
		//if (rone_diff > rone_wc) rone_wc = rone_diff;
		//rtt_total += rtt_diff;
		//one_total += one_diff;
		//rone_total += rone_diff;

		iters++;
		if (iters >= TEST_IPC_ITERS)
#ifndef TEST_IPI_BATCHING
		{
			break;
		}
#else
		{
			print_ipi_info();
			iters = 0;
		}
#endif
	}

	testing = 0;
	//print stats..
//	PRINTC("IPC RTT = AVG: %llu, WC: %llu, ITERS: %llu\n", rtt_total / iters, rtt_wc, iters);
//	PRINTC("IPC ONEWAY = AVG: %llu, WC: %llu, ITERS: %llu\n", one_total / iters, one_wc, iters);
//	PRINTC("IPC ONEWAY (RET) = AVG: %llu, WC: %llu, ITERS: %llu\n", rone_total / iters, rone_wc, iters);
#ifdef TEST_IPC
#ifndef TEST_IPI_BATCHING
	perfdata_calc(&pd[0]);
	perfdata_print(&pd[0]);

	PRINTC("Next set of data\n");
	perfdata_calc(&pd[1]);
	perfdata_print(&pd[1]);

//	perfdata_calc(&pd[2]);
//	perfdata_print(&pd[2]);
#endif
#endif
	print_ipi_info();

#ifdef TEST_SIMPLE_SCHED
	simple_sl_thd_exit();
#endif
	sl_thd_exit();
}

static void
c1_ipc_fn(arcvcap_t r, void *d)
{
	asndcap_t snd = cn_c0_asnd[cos_cpuid()];

	assert(snd);
#ifdef TEST_IPI_BATCHING
	cos_asnd(snd, 0);
#endif
	while (testing == 0) ;

	while (1) {
		int pending = 0, rcvd = 0, ret = 0;

		if (unlikely(testing == 0)) break;

		rdtscll(c1_start);
		pending = cos_rcv(r, RCV_ALL_PENDING, &rcvd);
		assert(pending == 0 && rcvd >= 1);

		rdtscll(c1_mid);
#ifndef TEST_IPI_BATCHING
		ret = cos_asnd(snd, 0);
		assert(ret == 0);
		rdtscll(c1_end);
#endif
	}

#ifdef TEST_SIMPLE_SCHED
	simple_sl_thd_exit();
#endif
	sl_thd_exit();
}

static void
test_ipc_setup(void)
{
#ifdef TEST_IPC
	static volatile int cdone[NUM_CPU] = { 0 };
	int i, ret;
	struct sl_thd *t = NULL;
	asndcap_t snd = 0;

	assert(NUM_CPU == 2); /* use only 2 cores for this test! */

	if (cos_cpuid() == 0) {
		t = sl_thd_aep_alloc(c0_ipc_fn, (void *)cos_cpuid(), 1, 0, 0, 0);
		assert(t);
#ifdef TEST_SIMPLE_SCHED
		simple_sl_thd_init_ext(sl_thd_thdid(t), 0, sl_thd_thdcap(t), sl_thd_rcvcap(t), 0);
#endif
		c0_rcv[cos_cpuid()] = sl_thd_rcvcap(t);

		while (!cn_rcv[1]) ;

#ifdef TEST_IPC_RAW
		snd = capmgr_asnd_rcv_create_raw(cn_rcv[1]);
#else
		snd = capmgr_asnd_rcv_create(cn_rcv[1]);
#endif
		assert(snd);
		c0_cn_asnd[cos_cpuid()] = snd;
	} else {
		t = sl_thd_aep_alloc(c1_ipc_fn, (void *)cos_cpuid(), 1, 0, 0, 0);
		assert(t);
#ifdef TEST_SIMPLE_SCHED
		simple_sl_thd_init_ext(sl_thd_thdid(t), 0, sl_thd_thdcap(t), sl_thd_rcvcap(t), 0);
#endif
		cn_rcv[cos_cpuid()] = sl_thd_rcvcap(t);

		while (!c0_rcv[0]) ;

#ifdef TEST_IPC_RAW
		snd = capmgr_asnd_rcv_create_raw(c0_rcv[0]);
#else
		snd = capmgr_asnd_rcv_create(c0_rcv[0]);
#endif
		assert(snd);
		cn_c0_asnd[cos_cpuid()] = snd;
	}

	ret = cos_tcap_transfer(sl_thd_rcvcap(t), BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, LOW_PRIO);
	assert(ret == 0);
	sl_thd_param_set(t, sched_param_pack(SCHEDP_WINDOW, AEP_PERIOD_US));
	sl_thd_param_set(t, sched_param_pack(SCHEDP_BUDGET, AEP_BUDGET_US));
	sl_thd_param_set(t, sched_param_pack(SCHEDP_PRIO, LOW_PRIO));

	ps_faa((unsigned long *)&cdone[cos_cpuid()], 1);
	for (i = 0; i < NUM_CPU; i++) {
		while (!ps_load((unsigned long *)&cdone[i])) ;
	}
#endif
}

#undef MICRO_IPI_FIRST_RUN

void
cos_init(void)
{
	int i;
	static unsigned long first = NUM_CPU + 1, init_done[NUM_CPU] = { 0 };
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci    = cos_compinfo_get(defci);

	assert(NUM_CPU > 1);

	if (ps_cas(&first, NUM_CPU + 1, cos_cpuid())) {
		PRINTC("In MICRO_IPI COMPONENT\n");
		cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
		cos_defcompinfo_init();

#ifndef TEST_IPC
#ifdef MICRO_IPI_FIRST_RUN
		PRINTC("MAKE SURE YOU MODIFY SL FOR THE FOLLOWING TEST TO WORK WELL\n");
		PRINTC("MAKE SURE YOU DON'T RUN IDLE THREAD IF NO THREAD IS READY TO RUN\n");
		PRINTC("SET TIMEOUT PARAM IN ALL COS_SWITCH TO 0. THIS IS DONE BY SETTING timeout_next to 0 IN SL\n");
		PRINTC("ONCE YOU DO ALL THAT, UNDEF FIRST_RUN AND RECOMPILE. :-)\n");
		assert(0);
#endif
#endif
	} else {
		while (!ps_load(&init_done[first])) ;

		cos_defcompinfo_sched_init();
	}
	ps_faa(&init_done[cos_cpuid()], 1);
	for (i = 0; i < NUM_CPU; i++) {
		while (!ps_load(&init_done[i])) ;
	}

#ifdef TEST_SIMPLE_SCHED
	simple_sl_init();
#endif
	sl_init(SCHED_PERIOD_US);
        hypercall_comp_init_done();

	//test_ipc_setup();
	//test_latency_setup();
	test_rate_setup();

#ifdef TEST_IPC
#ifndef TEST_SIMPLE_SCHED
	sl_sched_loop_nonblock();
#else
	simple_sl_sched_loop_nonblock();
#endif
#else
	sl_sched_loop_nonblock();
#endif

	assert(0);

	return;
}

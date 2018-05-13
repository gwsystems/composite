#include <stdint.h>

#include "micro_booter.h"

/* only one of the following tests must be enable at a time */
/* each core snd to all other cores through N threads.. and rcv from n threads.. */
#undef TEST_N_TO_N
/* one core has 1 thread on rcv.. all other cores just asnd to that one core */
#undef TEST_N_TO_1
/* just one core asnd to just another rcv on other core.. all other cores do nothing */
#define TEST_1_TO_1
#define TEST_RT

extern unsigned int cyc_per_usec;

#if defined(TEST_N_TO_N)
static volatile arcvcap_t test_rcvs[NUM_CPU][NUM_CPU];
static volatile asndcap_t test_asnds[NUM_CPU][NUM_CPU];
static volatile thdcap_t  test_rthds[NUM_CPU][NUM_CPU];
static volatile thdid_t   test_rtids[NUM_CPU][NUM_CPU];
static volatile int       test_thd_blkd[NUM_CPU][NUM_CPU];
#define MIN_THRESH 1000
#define TEST_IPI_ITERS 1000

static void
test_ipi_fn(void *d)
{
	asndcap_t snd = test_asnds[cos_cpuid()][(int)d];
	arcvcap_t rcv = test_rcvs[cos_cpuid()][(int)d];

	assert(snd && rcv);
	while (1) {
		int r = 0, p = 0;

		r = cos_asnd(snd, 1);
		assert(r == 0);
		p = cos_rcv(rcv, RCV_ALL_PENDING, &r);
		assert(p >= 0);
	}
}

static void
test_rcv_crt(void)
{
	int i;
	static volatile int rcv_crt[NUM_CPU] = { 0 };

	memset((void *)test_rcvs[cos_cpuid()], 0, NUM_CPU * sizeof(arcvcap_t));
	memset((void *)test_asnds[cos_cpuid()], 0, NUM_CPU * sizeof(asndcap_t));
	memset((void *)test_rthds[cos_cpuid()], 0, NUM_CPU * sizeof(thdcap_t));
	memset((void *)test_rtids[cos_cpuid()], 0, NUM_CPU * sizeof(thdid_t));
	memset((void *)test_thd_blkd[cos_cpuid()], 0, NUM_CPU * sizeof(int));

	for (i = 0; i < NUM_CPU; i++) {
		thdcap_t thd = 0;
		arcvcap_t rcv = 0;
		asndcap_t snd = 0;

		if (cos_cpuid() == i) continue;
		thd = cos_thd_alloc(&booter_info, booter_info.comp_cap, test_ipi_fn, (void *)i);
		assert(thd);

		rcv = cos_arcv_alloc(&booter_info, thd, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, booter_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE);
		assert(rcv);

		test_rcvs[cos_cpuid()][i] = rcv;
		test_rthds[cos_cpuid()][i] = thd;
		test_rtids[cos_cpuid()][i] = cos_introspect(&booter_info, thd, THD_GET_TID);
	}
	rcv_crt[cos_cpuid()] = 1;

	/* wait for rcvcaps to be created on all cores */
	for (i = 0; i < NUM_CPU; i++) {
		while (!rcv_crt[i]) ;
	}
}

static int
test_find_tid(thdid_t tid)
{
	int i = 0, r = -1;

	for (i = 0; i < NUM_CPU; i++) {
		if (test_rtids[cos_cpuid()][i] != tid) continue;

		r = i;
		break;
	}

	return r;
}

static void
test_asnd_crt(void)
{
	int i;
	static volatile int snd_crt[NUM_CPU] = { 0 };

	for (i = 0; i < NUM_CPU; i++) {
		arcvcap_t rcv = 0;
		asndcap_t snd = 0;

		if (i == cos_cpuid()) continue;
		rcv = test_rcvs[i][cos_cpuid()];
		snd = cos_asnd_alloc(&booter_info, rcv, booter_info.captbl_cap);
		assert(snd);

		test_asnds[cos_cpuid()][i] = snd;
	}
	snd_crt[cos_cpuid()] = 1;

	/* wait for sndcaps to be created on all cores for all cores */
	for (i = 0; i < NUM_CPU; i++) {
		while (!snd_crt[i]) ;
	}
}

static void
test_thd_act(void)
{
	int i, ret;

	for (i = 0; i < NUM_CPU; i ++) {
		if (i == cos_cpuid()) continue;
		if (test_thd_blkd[cos_cpuid()][i]) continue;

		do {
			ret = cos_switch(test_rthds[cos_cpuid()][i], BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, 0, 0, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, cos_sched_sync());
		} while (ret == -EAGAIN);
		if (ret == -EBUSY) break;
	}
}

void
test_ipi_full(void)
{
	int i;
	volatile cycles_t now, prev, total = 0, wc = 0;
	unsigned long *blk[NUM_CPU];

	if (NUM_CPU == 1) {
		blk[cos_cpuid()] = NULL;

		return;
	}

	test_rcv_crt();
	test_asnd_crt();

	for (i = 0; i < NUM_CPU; i++) {
		if (i == cos_cpuid()) blk[i] = NULL;
		else                  blk[i] = (unsigned long *)&test_thd_blkd[cos_cpuid()][i];
	}
	PRINTC("Start scheduling the threads on this core\n");

	rdtscll(now);
	prev = now;
	while (1) {
		int blocked, rcvd, pending;
		cycles_t cycles;
		tcap_time_t timeout, thd_timeout;
		thdid_t tid;
		int j;

		rdtscll(now);
		if (now - prev > MIN_THRESH) total += now - prev;
		if (now - prev > wc) wc = now - prev;
		test_thd_act();

		while ((pending = cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, RCV_ALL_PENDING, 0,
			     &rcvd, &tid, &blocked, &cycles, &thd_timeout)) >= 0) {
			if (!tid) goto done;
			j = test_find_tid(tid);
			assert(j >= 0);

			assert(blk[j]);
			*(blk[j]) = blocked;

done:			if(!pending) break;
		}
		rdtscll(prev);
	}

	assert(0);
}
#elif defined(TEST_N_TO_1) || defined(TEST_1_TO_1)

#define TEST_RCV_CORE 0
#define TEST_SND_CORE 1
#define TEST_IPI_ITERS 1000000

volatile asndcap_t asnd[NUM_CPU] = { 0 };
volatile arcvcap_t rcv[NUM_CPU] = { 0 };
volatile thdcap_t  thd[NUM_CPU] = { 0 };
volatile thdid_t   tid[NUM_CPU] = { 0 };
volatile int       blkd[NUM_CPU] = { 0 };

volatile unsigned long long total_rcvd[NUM_CPU] = { 0 };
volatile unsigned long long total_sent[NUM_CPU] = { 0 };

static void
test_rcv(arcvcap_t r)
{
	int pending = 0, rcvd = 0;

	pending = cos_rcv(r, RCV_ALL_PENDING, &rcvd);
	assert(pending == 0);

	total_rcvd[cos_cpuid()] += rcvd;
}

static void
test_asnd(asndcap_t s)
{
	int ret = 0;

	ret = cos_asnd(s, 1);
	assert(ret == 0 || ret == -EBUSY);
	if (!ret) total_sent[cos_cpuid()]++;
}

static void
test_rcv_fn(void *d)
{
	arcvcap_t r = rcv[cos_cpuid()];
	asndcap_t s = asnd[cos_cpuid()];

	while (1) {
		test_rcv(r);

#if defined(TEST_1_TO_1) && defined(TEST_RT)
		test_asnd(s);
#endif
	}
}

static void
test_sched_loop(void)
{
	while (1) {
		int blocked, rcvd, pending, ret;
		cycles_t cycles;
		tcap_time_t timeout, thd_timeout;
		thdid_t thdid;

		while ((pending = cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, RCV_ALL_PENDING, 0,
						&rcvd, &thdid, &blocked, &cycles, &thd_timeout)) >= 0) {
			if (!thdid) goto done;
			assert(thdid == tid[cos_cpuid()]);
			blkd[cos_cpuid()] = blocked;
done:
			if (!pending) break;
		}

		if (blkd[cos_cpuid()]) continue;

		do {
			ret = cos_switch(thd[cos_cpuid()], BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, 0, 0, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, cos_sched_sync());
		} while (ret == -EAGAIN);
	}

}

static void
test_rcv_main(void)
{
	test_sched_loop();
}

static void
test_asnd_fn(void *d)
{
#if defined(TEST_1_TO_1) && defined(TEST_RT)
	cycles_t st = 0, en = 0, tot = 0, wc = 0, pwc = 0;
	int iters = 0, iterct = 0;
#endif
	arcvcap_t r = rcv[cos_cpuid()];
	asndcap_t s = asnd[cos_cpuid()];

	while (1) {
#if defined(TEST_1_TO_1) && defined(TEST_RT)
		rdtscll(st);
#endif
		test_asnd(s);

#if defined(TEST_1_TO_1) && defined(TEST_RT)
		test_rcv(r);
		rdtscll(en);

		tot += (en - st);
		if (en - st > wc) {
			pwc = wc;
			wc = en - st;
		}
		iters ++;
		if (iters >= TEST_IPI_ITERS) {
			PRINTC("<%d> Average: %llu (T:%llu, I:%d), WC: %llu (p:%llu) ",
			       iterct, (tot / iters) / 2, tot, iters * 2, wc, pwc);
			PRINTC("[Rcvd: %llu, Sent: %llu]\n", total_rcvd[TEST_RCV_CORE] + total_rcvd[TEST_SND_CORE], total_sent[TEST_RCV_CORE] + total_rcvd[TEST_RCV_CORE]);
			wc = pwc = 0; /* FIXME: for test */
			iters = 0;
			tot = 0;
			iterct ++;
		}
#endif
	}
}

static void
test_asnd_main(void)
{
#if defined(TEST_1_TO_1) && defined(TEST_RT)
	test_sched_loop();
#else
	test_asnd_fn(NULL);
#endif
}

static void
test_sync_asnd(void)
{
	int i;

#if defined(TEST_N_TO_1)
	for (i = 0; i < NUM_CPU; i++) {
		if (i == TEST_RCV_CORE) continue;
		while (!asnd[i]) ;
	}
#else
	while (!asnd[TEST_SND_CORE]) ;
#if defined(TEST_RT)
	while (!asnd[TEST_RCV_CORE]) ;
#endif
#endif
}

void
test_ipi_full(void)
{
	arcvcap_t r = 0;
	asndcap_t s = 0;
	thdcap_t t = 0;

	if (cos_cpuid() == TEST_RCV_CORE) {
		t = cos_thd_alloc(&booter_info, booter_info.comp_cap, test_rcv_fn, NULL);
		assert(t);

		r = cos_arcv_alloc(&booter_info, t, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, booter_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE);
		assert(r);

		thd[cos_cpuid()] = t;
		tid[cos_cpuid()] = cos_introspect(&booter_info, t, THD_GET_TID);
		rcv[cos_cpuid()] = r;
#if defined(TEST_1_TO_1) && defined(TEST_RT)
		while(!rcv[TEST_SND_CORE]) ;

		s = cos_asnd_alloc(&booter_info, rcv[TEST_SND_CORE], booter_info.captbl_cap);
		assert(s);
		asnd[cos_cpuid()] = s;
#endif
		test_sync_asnd();
		test_rcv_main();
	} else {
#if defined(TEST_1_TO_1)
		if (cos_cpuid() != TEST_SND_CORE) return;

#if defined(TEST_RT)
		t = cos_thd_alloc(&booter_info, booter_info.comp_cap, test_asnd_fn, NULL);
		assert(t);

		r = cos_arcv_alloc(&booter_info, t, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, booter_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE);
		assert(r);

		thd[cos_cpuid()] = t;
		tid[cos_cpuid()] = cos_introspect(&booter_info, t, THD_GET_TID);
		rcv[cos_cpuid()] = r;
#endif
#endif
		while(!rcv[TEST_RCV_CORE]) ;

		s = cos_asnd_alloc(&booter_info, rcv[TEST_RCV_CORE], booter_info.captbl_cap);
		assert(s);
		asnd[cos_cpuid()] = s;

		test_sync_asnd();
		test_asnd_main();
	}
}
#endif

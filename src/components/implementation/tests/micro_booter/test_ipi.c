#include <stdint.h>

#include "micro_booter.h"

extern int _expect_llu(int predicate, char *str, long long unsigned a, long long unsigned b, char *errcmp, char *testname, char *file, int line);
extern int _expect_ll(int predicate, char *str, long long a, long long b, char *errcmp, char *testname, char *file, int line);

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

#define MAX_THRS 4
#define MIN_THRS 1

volatile thdcap_t  spinner_thd[NUM_CPU] = { 0 };
volatile cycles_t  global_time[2] = { 0 };

volatile int       done_test = 0;
volatile int       ret_enable = 1;
volatile int       pending_rcv = 0;

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

		if (blkd[cos_cpuid()] && done_test) return;
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
	cycles_t st = 0, en = 0, tot = 0, wc = 0, pwc = 0, bc = 0, s_time = 0;
    cycles_t tot_send = 0, send_wc = 0;
	int iters = 0;
	arcvcap_t r = rcv[cos_cpuid()];
	asndcap_t s = asnd[cos_cpuid()];

    while(1) {
        rdtscll(st);
        test_asnd(s);

        rdtscll(s_time);
        test_rcv(r);
        rdtscll(en);

        tot_send += (s_time - st);
        if (en - st > send_wc) {
            wc = s_time - st;
        }

        tot += (en - st);
        if (en - st > wc) {
            pwc = wc;
            wc = en - st;
        }
        if (en - st < bc || bc == 0) {
            bc = en - st;
        }
        iters ++;
        if (iters >= TEST_IPI_ITERS) {
            break;
//                PRINTC("<%d> Average: %llu (T:%llu, I:%d), WC: %llu (p:%llu) ",
//                       iterct, (tot / iters) / 2, tot, iters * 2, wc, pwc);
//                PRINTC("[Rcvd: %llu, Sent: %llu]\n", total_rcvd[TEST_RCV_CORE] + total_rcvd[TEST_SND_CORE], total_sent[TEST_RCV_CORE] + total_rcvd[TEST_RCV_CORE]);
        }
    }

    PRINTC("LOL WC: %llu, %llu, %llu\n", bc/2, wc/2, tot/TEST_IPI_ITERS/2);
    EXPECT_LLU_LT((long long unsigned)((tot_send / TEST_IPI_ITERS) * MAX_THRS), wc, "Test IPI Multi-Core MAX");
    EXPECT_LLU_LT((long long unsigned)((tot / TEST_IPI_ITERS) / 2 * MAX_THRS), wc / 2, "Test IPI Multi-Core MAX");
    EXPECT_LLU_LT(bc / 2 * (unsigned) MIN_THRS, (long long unsigned)((tot / TEST_IPI_ITERS) / 2), "Test IPI Multi-Core MIN");
//test
    done_test = 1;
    while(1) test_rcv(r);
    //    while(1);
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

// RCV TEST #1

static void
rcv_spinner(void *d)
{
    while (!done_test){
        rdtscll(global_time[0]);
    }

    while (1) cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);
}

static void
test_rcv_1(arcvcap_t r)
{
	int pending = 0, rcvd = 0;

	pending = cos_rcv(r, RCV_ALL_PENDING, &rcvd);
    rdtscll(global_time[1]);
    assert(pending == 0);

	total_rcvd[cos_cpuid()] += rcvd;
}

static void
test_rcv_fn_1(void *d)
{
	arcvcap_t r = rcv[cos_cpuid()];
	asndcap_t s = asnd[cos_cpuid()];

	while (1) {
//        while(pending_rcv);
		test_rcv_1(r);
#if defined(TEST_1_TO_1) && defined(TEST_RT)
		test_asnd(s);
#endif
	}
}

static void
test_asnd_fn_1(void *d)
{
    PRINTC("TEST %d\n",__LINE__);
    cycles_t tot = 0, wc = 0, bc = 0;
    int iters = 0;

    arcvcap_t r = rcv[cos_cpuid()];
    asndcap_t s = asnd[cos_cpuid()];

    for(iters = 0; iters < TEST_IPI_ITERS; iters++) {
//        while(ret_enable);

        test_asnd(s);
        test_rcv(r);

        tot += (global_time[1] - global_time[0]);
        if (global_time[1] - global_time[0] > 0) {
            wc = global_time[1] - global_time[0];
        }
        if (global_time[1] - global_time[0] < bc || bc == 0){
            bc = global_time[1] - global_time[0];
        }
    }
    PRINTC("LOL WC: %llu, %llu, %llu\n", bc, wc, tot/TEST_IPI_ITERS);
    EXPECT_LLU_LT((long long unsigned)((tot / TEST_IPI_ITERS) * MAX_THRS), wc , "Test IPI Multi-Core MAX");
    EXPECT_LLU_LT(bc * (unsigned) MIN_THRS, (long long unsigned)(tot / TEST_IPI_ITERS), "Test IPI Multi-Core MIN");
    done_test = 1;
    while(1) test_rcv(r);
}

static void
test_sched_loop_1(void)
{
	while (1) {
		int blocked, rcvd, pending, ret;
		cycles_t cycles;
		tcap_time_t timeout, thd_timeout;
		thdid_t thdid;

        if(cos_cpuid() == TEST_RCV_CORE){
            do {
                ret = cos_switch(spinner_thd[cos_cpuid()], BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_PRIO_MAX + 2, 0, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, cos_sched_sync());
            } while (ret == -EAGAIN);
        }
        while ((pending = cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, RCV_ALL_PENDING, 0,
						&rcvd, &thdid, &blocked, &cycles, &thd_timeout)) >= 0) {
			if (!thdid) goto done;
			assert(thdid == tid[cos_cpuid()]);
//            if (EXPECT_LL_NEQ(thdid, tid[cos_cpuid()], "Multicore IPI"))
//                return;
            blkd[cos_cpuid()] = blocked;
done:
			if (!pending) break;
		}

		if (blkd[cos_cpuid()] && done_test) return;

        if(cos_cpuid() == TEST_SND_CORE && blkd[cos_cpuid()]) continue;

        if(cos_cpuid() == TEST_SND_CORE) {
            do {
                ret = cos_switch(thd[cos_cpuid()], BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, 0, 0, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, cos_sched_sync());
            } while (ret == -EAGAIN);
        }
	}

}



// RCV TEST #2

static void
rcv_spinner_2(void *d)
{
    int i = 0;
    while (!done_test){
        rdtscll(global_time[(i++%2)]);
    }

    PRINTC("D\n");
    while (1) cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);
}

static void
test_asnd_fn_2(void *d)
{
    PRINTC("TEST %d\n",__LINE__);
    cycles_t tot = 0, mask = 0, time = 0, wc = 0, bc = 0;
    int iters = 0;

    arcvcap_t r = rcv[cos_cpuid()];
    asndcap_t s = asnd[cos_cpuid()];

    for(iters = 0; iters < TEST_IPI_ITERS; iters++) {

        test_asnd(s);
        time = global_time[1] - global_time[0];

        // Fast ABS()

        mask = (time >> (sizeof(int) * CHAR_BIT - 1));
        time = (time + mask) ^ mask;

        if(time != 28)
            PRINTC("TIME: %llu\n", time);

//        test_asnd(s);
//        test_rcv(r);

        tot += time;
        if (time > wc) {
            wc = time;
        }
        if ( time < bc || bc == 0){
            bc = time;
        }
    }
    PRINTC("LOL WC: %llu, %llu, %llu\n", bc, wc, tot/TEST_IPI_ITERS);
    EXPECT_LLU_LT((long long unsigned)((tot / TEST_IPI_ITERS) * MAX_THRS), wc , "Test IPI Multi-Core MAX");
    EXPECT_LLU_LT(bc * (unsigned) MIN_THRS, (long long unsigned)(tot / TEST_IPI_ITERS), "Test IPI Multi-Core MIN");
    done_test = 1;
    while(1) test_rcv(r);
}

void
test_ipi_full(void)
{
	arcvcap_t r = 0;
	asndcap_t s = 0;
	thdcap_t t = 0;

	if (cos_cpuid() == TEST_RCV_CORE) {
/*		t = cos_thd_alloc(&booter_info, booter_info.comp_cap, test_rcv_fn, NULL);
		assert(t);

		r = cos_arcv_alloc(&booter_info, t, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, booter_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE);
		assert(r);

		thd[cos_cpuid()] = t;
		tid[cos_cpuid()] = cos_introspect(&booter_info, t, THD_GET_TID);
		rcv[cos_cpuid()] = r;
		while(!rcv[TEST_SND_CORE]) ;
        done_test = 0;

		s = cos_asnd_alloc(&booter_info, rcv[TEST_SND_CORE], booter_info.captbl_cap);
		assert(s);
		asnd[cos_cpuid()] = s;
		test_sync_asnd();
//		test_rcv_main();
        test_sched_loop();

        PRINTC("TEST 1\n");
        while(1);

        rcv[TEST_SND_CORE] = 0;

        // Test RCV 1: Close Loop at lower priority => Measure route with switching

        t = cos_thd_alloc(&booter_info, booter_info.comp_cap, test_rcv_fn_1, NULL);
		assert(t);

		r = cos_arcv_alloc(&booter_info, t, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, booter_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE);
		assert(r);

        cos_tcap_transfer(r, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, TCAP_PRIO_MAX);

		thd[cos_cpuid()] = t;
		tid[cos_cpuid()] = cos_introspect(&booter_info, t, THD_GET_TID);
		rcv[cos_cpuid()] = r;
		while(!rcv[TEST_SND_CORE]) ;
        done_test = 0;

        t = cos_thd_alloc(&booter_info, booter_info.comp_cap, rcv_spinner, NULL);
        assert(t);

        spinner_thd[cos_cpuid()] = t;

		s = cos_asnd_alloc(&booter_info, rcv[TEST_SND_CORE], booter_info.captbl_cap);
		assert(s);
		asnd[cos_cpuid()] = s;
		test_sync_asnd();
        test_sched_loop_1();

        rcv[TEST_SND_CORE] = 0;
        global_time[0] = 0;
        global_time[1] = 0;
*/
        // Test RCV 2: Close Loop at higher priority => Measure Kernel involvement

        t = cos_thd_alloc(&booter_info, booter_info.comp_cap, test_rcv_fn_1, NULL);
		assert(t);

		r = cos_arcv_alloc(&booter_info, t, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, booter_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE);
		assert(r);

        cos_tcap_transfer(r, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, TCAP_PRIO_MAX + 5);

		thd[cos_cpuid()] = t;
		tid[cos_cpuid()] = cos_introspect(&booter_info, t, THD_GET_TID);
		rcv[cos_cpuid()] = r;
		while(!rcv[TEST_SND_CORE]) ;
        done_test = 0;

        t = cos_thd_alloc(&booter_info, booter_info.comp_cap, rcv_spinner_2, NULL);
        assert(t);

        spinner_thd[cos_cpuid()] = t;

		s = cos_asnd_alloc(&booter_info, rcv[TEST_SND_CORE], booter_info.captbl_cap);
		assert(s);
		asnd[cos_cpuid()] = s;
		test_sync_asnd();
//        PRINTC("TEST %d\n",__LINE__);
        test_sched_loop_1();
//        PRINTC("WAS %d\n",__LINE__);
        while(1);

    } else {

		if (cos_cpuid() != TEST_SND_CORE) return;
/*
		t = cos_thd_alloc(&booter_info, booter_info.comp_cap, test_asnd_fn, NULL);
		assert(t);

		r = cos_arcv_alloc(&booter_info, t, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, booter_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE);
		assert(r);

		thd[cos_cpuid()] = t;
		tid[cos_cpuid()] = cos_introspect(&booter_info, t, THD_GET_TID);
		rcv[cos_cpuid()] = r;
		while(!rcv[TEST_RCV_CORE]) ;
        while(done_test);

		s = cos_asnd_alloc(&booter_info, rcv[TEST_RCV_CORE], booter_info.captbl_cap);
		assert(s);
		asnd[cos_cpuid()] = s;

		test_sync_asnd();
//		test_asnd_main();
		test_sched_loop();

        PRINTC("TEST 2\n");
        while(1);


        rcv[TEST_RCV_CORE] = 0;

        // Test RCV1: Corresponding Send

        t = cos_thd_alloc(&booter_info, booter_info.comp_cap, test_asnd_fn_1, NULL);
		assert(t);

		r = cos_arcv_alloc(&booter_info, t, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, booter_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE);
		assert(r);

		thd[cos_cpuid()] = t;
		tid[cos_cpuid()] = cos_introspect(&booter_info, t, THD_GET_TID);
		rcv[cos_cpuid()] = r;
		while(!rcv[TEST_RCV_CORE]) ;
        while(done_test);

		s = cos_asnd_alloc(&booter_info, rcv[TEST_RCV_CORE], booter_info.captbl_cap);
		assert(s);
		asnd[cos_cpuid()] = s;

		test_sync_asnd();
//		test_asnd_main();
		test_sched_loop_1();


        rcv[TEST_RCV_CORE] = 0;
*/
        // Test RCV2: Corresponding Send

        t = cos_thd_alloc(&booter_info, booter_info.comp_cap, test_asnd_fn_2, NULL);
		assert(t);

		r = cos_arcv_alloc(&booter_info, t, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, booter_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE);
		assert(r);

		thd[cos_cpuid()] = t;
		tid[cos_cpuid()] = cos_introspect(&booter_info, t, THD_GET_TID);
		rcv[cos_cpuid()] = r;
		while(!rcv[TEST_RCV_CORE]) ;
        while(done_test);

		s = cos_asnd_alloc(&booter_info, rcv[TEST_RCV_CORE], booter_info.captbl_cap);
		assert(s);
		asnd[cos_cpuid()] = s;

		test_sync_asnd();
//		test_asnd_main();
        PRINTC("TEST %d\n",__LINE__);
		test_sched_loop_1();
        PRINTC("TEST %d\n",__LINE__);
        while(1);
	}
}

#endif

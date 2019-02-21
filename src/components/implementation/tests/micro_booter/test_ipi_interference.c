#include <stdint.h>

#include "micro_booter.h"

extern int _expect_llu(int predicate, char *str, long long unsigned a, long long unsigned b, char *errcmp, char *testname, char *file, int line);
extern int _expect_ll(int predicate, char *str, long long a, long long b, char *errcmp, char *testname, char *file, int line);

extern void clear_sched(int* rcvd, thdid_t* tid, int* blocked, cycles_t* cycles, tcap_time_t* thd_timeout);

/* only one of the following tests must be enable at a time */
/* each core snd to all other cores through N threads.. and rcv from n threads.. */
/* one core has 1 thread on rcv.. all other cores just asnd to that one core */

/* just one core asnd to just another rcv on other core.. all other cores do nothing */

extern unsigned int cyc_per_usec;

#define TEST_RCV_CORE 0
#define TEST_SND_CORE 1
#define TEST_IPI_ITERS 1000000

static volatile asndcap_t asnd[NUM_CPU] = { 0 };
static volatile arcvcap_t rcv[NUM_CPU] = { 0 };
static volatile thdcap_t  thd[NUM_CPU] = { 0 };
static volatile thdid_t   tid[NUM_CPU] = { 0 };
static volatile int       blkd[NUM_CPU] = { 0 };

static volatile unsigned long long total_rcvd[NUM_CPU] = { 0 };
static volatile unsigned long long total_sent[NUM_CPU] = { 0 };

#define MAX_THRS 4
#define MIN_THRS 1

static volatile thdcap_t  spinner_thd[NUM_CPU] = { 0 };
static volatile cycles_t  global_time[2] = { 0 };

static volatile int       done_test = 0;
static volatile int       ret_enable = 1;
static volatile int       pending_rcv = 0;

static void
test_rcv(arcvcap_t r)
{
	int pending = 0, rcvd = 0;

//    assert(0);

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
test_sync_asnd(void)
{
//	int i;

//#if defined(TEST_N_TO_1)
//	for (i = 0; i < NUM_CPU; i++) {
//		if (i == TEST_RCV_CORE) continue;
//		while (!asnd[i]) ;
//	}
//#else
	while (!asnd[TEST_SND_CORE]) ;
//#if defined(TEST_RT)
	while (!asnd[TEST_RCV_CORE]) ;
//#endif
//#endif
}

// RCV TEST #1

static void
test_rcv_fn(void *d)
{
	arcvcap_t r = rcv[cos_cpuid()];
	asndcap_t s = asnd[cos_cpuid()];

	while (1) {
//        while(pending_rcv);
		test_rcv(r);
//#if defined(TEST_1_TO_1) && defined(TEST_RT)
		test_asnd(s);
//#endif
	}
}

static void
test_sched_loop(void)
{
    int blocked, rcvd, pending, ret;
    cycles_t cycles;
    tcap_time_t timeout, thd_timeout;
    thdid_t thdid;

    // Clear Scheduler
    clear_sched(&rcvd, &thdid, &blocked, &cycles, &thd_timeout);

    while (1) {

        if(cos_cpuid() == TEST_RCV_CORE){
            do {
                ret = cos_switch(spinner_thd[cos_cpuid()], BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_PRIO_MAX + 2, 0, 0, 0);
//                PRINTC("NFL: %d\n", ret);
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
                ret = cos_switch(thd[cos_cpuid()], BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, 0, 0, 0, 0);
            } while (ret == -EAGAIN);
        }
	}

}

// RCV TEST #2

static void
rcv_spinner(void *d)
{
	arcvcap_t r = rcv[cos_cpuid()];
	asndcap_t s = asnd[cos_cpuid()];
	int ret = 0;

    PRINTC("D\n");
//    test_asnd(s);

	ret = cos_asnd(s, 0);
	assert(ret == 0 || ret == -EBUSY);

    PRINTC("ST\n");

    int i = 0;
    cycles_t now = 0, prev = 0, tot_avg = 0;
    rdtscll(now);
    prev = now;
    while (!done_test){
          rdtscll(now);
//        rdtscll(global_time[(i++%2)]);
/*          if((now - prev) > 30){
            tot_avg += now - prev;
              i++;
          }
          if(i > TEST_IPI_ITERS)
            break;
*/
          tot_avg += now - prev;
          prev = now;
    }

    PRINTC("D %llu %llu\n", tot_avg ,tot_avg / TEST_IPI_ITERS);
    while(!done_test);
    while (1) cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);
}

static void
test_asnd_fn(void *d)
{
//    PRINTC("TEST %d\n",__LINE__);
    cycles_t tot = 0, mask = 0, time = 0, wc = 0, bc = 0;
    int iters = 0;

    arcvcap_t r = rcv[cos_cpuid()];
    asndcap_t s = asnd[cos_cpuid()];


//        PRINTC("plp %d\n",__LINE__);
        test_rcv(r);
//        PRINTC("sls %d\n",__LINE__);

    for(iters = 0; iters < TEST_IPI_ITERS; iters++) {
//        for(;;){
        test_asnd(s);
        time = global_time[1] - global_time[0];

        // Fast ABS()

        mask = (time >> (sizeof(int) * CHAR_BIT - 1));
        time = (time + mask) ^ mask;

//        if(time != 28)
//            PRINTC("TIME: %llu\n", time);

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
//    PRINTC("LOLs WC: %llu, %llu, %llu\n", bc, wc, tot/TEST_IPI_ITERS);
//    EXPECT_LLU_LT((long long unsigned)((tot / TEST_IPI_ITERS) * MAX_THRS), wc , "Test IPI Multi-Core MAX");
//    EXPECT_LLU_LT(bc * (unsigned) MIN_THRS, (long long unsigned)(tot / TEST_IPI_ITERS), "Test IPI Multi-Core MIN");
    done_test = 1;
    while(1) test_rcv(r);
}

void
test_ipi_interference(void)
{
	arcvcap_t r = 0;
	asndcap_t s = 0;
	thdcap_t t = 0;
    tcap_t	tcc = 0;


	if (cos_cpuid() == TEST_RCV_CORE) {

//        rcv[TEST_SND_CORE] = 0;
//        global_time[0] = 0;
//        global_time[1] = 0;

        // Test RCV 2: Close Loop at higher priority => Measure Kernel involvement
        tcc = cos_tcap_alloc(&booter_info);
        assert(tcc);

        PRINTC("TESTES\n");

        t = cos_thd_alloc(&booter_info, booter_info.comp_cap, test_rcv_fn, NULL);
		assert(t);

		r = cos_arcv_alloc(&booter_info, t, tcc, booter_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE);
		assert(r);

        cos_tcap_transfer(r, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, TCAP_PRIO_MAX + 5);

		thd[cos_cpuid()] = t;
		tid[cos_cpuid()] = cos_introspect(&booter_info, t, THD_GET_TID);
		rcv[cos_cpuid()] = r;
        PRINTC("TEST %d\n",__LINE__);
		while(!rcv[TEST_SND_CORE]) ;
//        done_test = 0;

        t = cos_thd_alloc(&booter_info, booter_info.comp_cap, rcv_spinner, NULL);
        assert(t);

        spinner_thd[cos_cpuid()] = t;

		s = cos_asnd_alloc(&booter_info, rcv[TEST_SND_CORE], booter_info.captbl_cap);
		assert(s);
		asnd[cos_cpuid()] = s;
		test_sync_asnd();
        PRINTC("TEST %d\n",__LINE__);
        test_sched_loop();
//        PRINTC("WAS %d\n",__LINE__);
        while(1);

    } else {

		if (cos_cpuid() != TEST_SND_CORE) return;

  //      rcv[TEST_RCV_CORE] = 0;
        // Test RCV2: Corresponding Send

        t = cos_thd_alloc(&booter_info, booter_info.comp_cap, test_asnd_fn, NULL);
		assert(t);

		r = cos_arcv_alloc(&booter_info, t, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, booter_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE);
		assert(r);

		thd[cos_cpuid()] = t;
		tid[cos_cpuid()] = cos_introspect(&booter_info, t, THD_GET_TID);
		rcv[cos_cpuid()] = r;
		while(!rcv[TEST_RCV_CORE]) ;
 //       while(done_test);

		s = cos_asnd_alloc(&booter_info, rcv[TEST_RCV_CORE], booter_info.captbl_cap);
		assert(s);
		asnd[cos_cpuid()] = s;

		test_sync_asnd();
//		test_asnd_main();
//        PRINTC("TEST %d\n",__LINE__);
		test_sched_loop();
//        PRINTC("TEST %d\n",__LINE__);
        while(1);
	}
}

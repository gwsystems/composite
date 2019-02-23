#include <stdint.h>

#include "micro_booter.h"

extern int _expect_llu(int predicate, char *str, long long unsigned a, long long unsigned b, char *errcmp, char *testname, char *file, int line);
extern int _expect_ll(int predicate, char *str, long long a, long long b, char *errcmp, char *testname, char *file, int line);
extern void clear_sched(int* rcvd, thdid_t* tid, int* blocked, cycles_t* cycles, tcap_time_t* thd_timeout);

/* Test Sender Time + Receiver Time Roundtrip */

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

#define MAX_THRS 1
#define MIN_THRS 1

static volatile int       done_test = 0;

static void
test_rcv(arcvcap_t r)
{
	int pending = 0, rcvd = 0;

	pending = cos_rcv(r, RCV_ALL_PENDING, &rcvd);
	assert(pending == 0);
    if (EXPECT_LL_LT(1, r, "Allocation"))
        cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);

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
		test_asnd(s);
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
			ret = cos_switch(thd[cos_cpuid()], BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, 0, 0, 0, 0);
		} while (ret == -EAGAIN);
	}

}

static void
test_asnd_fn(void *d)
{
	cycles_t st = 0, en = 0, tot = 0, wc = 0, bc = 0;
    cycles_t tot_send = 0, send_wc = 0, s_time = 0;
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
            send_wc = s_time - st;
        }

        tot += (en - st);
        if (en - st > wc) {
            wc = en - st;
        }
        if (en - st < bc || bc == 0) {
            bc = en - st;
        }
        iters ++;
        if (iters >= TEST_IPI_ITERS) {
            break;
        }
    }

#if defined(PERF)
    PRINTC("SEND TIME: %llu, WC: %llu, Iter: %d\n", tot_send/TEST_IPI_ITERS, send_wc, TEST_IPI_ITERS);
    PRINTC("ROUNDTRIP: %llu, WC: %llu, BC: %llu, Iter %d\n", bc/2, wc/2, tot/TEST_IPI_ITERS/2, TEST_IPI_ITERS);
#endif
    EXPECT_LLU_LT((long long unsigned)((tot_send / TEST_IPI_ITERS) * MAX_THRS), send_wc, "Test IPI SEND TIME");
    EXPECT_LLU_LT((long long unsigned)((tot / TEST_IPI_ITERS) / 2 * MAX_THRS), wc / 2, "Test IPI ROUNDTRIP MAX");
    EXPECT_LLU_LT(bc / 2 * (unsigned) MIN_THRS, (long long unsigned)((tot / TEST_IPI_ITERS) / 2), "Test IPI ROUNDTRIP MIN");

    done_test = 1;
    while(1) test_rcv(r);
}

static void
test_sync_asnd(void)
{
	int i;

	while (!asnd[TEST_SND_CORE]) ;
	while (!asnd[TEST_RCV_CORE]) ;
}

void
test_ipi_roundtime(void)
{
    arcvcap_t r = 0;
    asndcap_t s = 0;
    thdcap_t t = 0;
    tcap_t  tcc = 0;


    if (cos_cpuid() == TEST_RCV_CORE) {

        // Test Sender Time + Receiver Time Roundtrip

        tcc = cos_tcap_alloc(&booter_info);
        if (EXPECT_LL_LT(1, tcc, "Allocation"))
            return;


        t = cos_thd_alloc(&booter_info, booter_info.comp_cap, test_rcv_fn, NULL);
        if (EXPECT_LL_LT(1, t, "Allocation"))
            return;

        r = cos_arcv_alloc(&booter_info, t, tcc, booter_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE);
        if (EXPECT_LL_LT(1, r, "Allocation"))
            return;

        cos_tcap_transfer(r, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, TCAP_PRIO_MAX);

        thd[cos_cpuid()] = t;
        tid[cos_cpuid()] = cos_introspect(&booter_info, t, THD_GET_TID);
        rcv[cos_cpuid()] = r;
        while(!rcv[TEST_SND_CORE]) ;

        s = cos_asnd_alloc(&booter_info, rcv[TEST_SND_CORE], booter_info.captbl_cap);
        if (EXPECT_LL_LT(1, s, "Allocation"))
            return;

        asnd[cos_cpuid()] = s;
        test_sync_asnd();
        test_sched_loop();

    } else {

        if (cos_cpuid() != TEST_SND_CORE) return;

        // Test Sender Time

        t = cos_thd_alloc(&booter_info, booter_info.comp_cap, test_asnd_fn, NULL);
        if (EXPECT_LL_LT(1, t, "Allocation"))
            return;

        r = cos_arcv_alloc(&booter_info, t, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, booter_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE);
        if (EXPECT_LL_LT(1, r, "Allocation"))
            return;

        thd[cos_cpuid()] = t;
        tid[cos_cpuid()] = cos_introspect(&booter_info, t, THD_GET_TID);
        rcv[cos_cpuid()] = r;
        while(!rcv[TEST_RCV_CORE]) ;

        s = cos_asnd_alloc(&booter_info, rcv[TEST_RCV_CORE], booter_info.captbl_cap);
        if (EXPECT_LL_LT(1, s, "Allocation"))
            return;

        asnd[cos_cpuid()] = s;

        test_sync_asnd();
        test_sched_loop();
    }
}

#include <stdint.h>

#include "micro_booter.h"

#define THD_ARG 666             /* Thread Argument to pass */
#define NUM_TEST 16             /* Iterator NUM */
#define MAX_THDS 4              /* Max Threshold Multiplier */
#define MIN_THDS 0.5              /* Min Threshold Multiplier */
#define GRANULARITY 1000        /* Granularity */

struct perfdata result_test_timer;
struct perfdata result_budgets_single;
struct perfdata result_sinv;

static int      failure = 0;

static void
test_thd_arg(void *d)
{
    int ret = 0;

    if (EXPECT_LL_NEQ((int)d, THD_ARG, "Thread Creation: Argument Incorrect")) failure = 1;
    while (1) cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);
    PRINTC("Error, shouldn't get here!\n");
}

static void
test_thds_create_switch(void)
{
    thdcap_t ts;
    intptr_t i = THD_ARG;
    int      ret;

    ts = cos_thd_alloc(&booter_info, booter_info.comp_cap, test_thd_arg, (void *)i);
    if (EXPECT_LL_LT(1, ts, "Thread Creation: Cannot Allocate")) {
        return;
    }
    ret = cos_thd_switch(ts);
    EXPECT_LL_NEQ(0, ret, "COS Switch Error");

    CHECK_STATUS_FLAG();
    PRINTC("\t%s: \t\t\tSuccess\n", "THD => Creation & ARG");
    EXIT_FN();
}

static void
thd_fn_mthds_ring(void *d)
{
    int ret;

    if (count != (int) d)
        cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);

    int next = (++count) % TEST_NTHDS;
    if (!next) cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);

    ret = cos_thd_switch(thd_test[next]);
    if (EXPECT_LL_NEQ(0, ret, "Thread Ring: COS Switch Error")) failure = 1;

    while (1) {
        cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);
    }
    PRINTC("Error, shouldn't get here!\n");
}

                /* Ring Multithreaded Test */
static void
test_mthds_ring(void)
{
    int   i, ret;

    count = 0;

    for (i = 0; i < TEST_NTHDS; i++) {
        thd_test[i] = cos_thd_alloc(&booter_info, booter_info.comp_cap, thd_fn_mthds_ring, (void *)i);
        if (EXPECT_LL_LT(1, thd_test[i], "Thread Ring: Cannot Allocate")) {
            return;
        }
    }

    ret = cos_thd_switch(thd_test[0]);
    EXPECT_LL_NEQ(0, ret, "Thread Ring: COS Switch Error");

    if (EXPECT_LL_NEQ(count, TEST_NTHDS, "Thread Ring: Failure # of THDS")) {
        return;
    }

    CHECK_STATUS_FLAG();
    PRINTC("\t%s: \t\t\tSuccess\n", "THD => Switch Cyclic" );
    EXIT_FN();
}

static void
thd_fn_mthds_classic(void *d)
{
    cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);

    while (1) {
        cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);
    }
    EXPECT_LL_NEQ(1, 0, "Error, shouldn't get here!\n");
}

                /* Classic Multithreaded Test */
static void
test_mthds_classic(void)
{
    thdcap_t  ts;
    int       i, ret;

    ts = cos_thd_alloc(&booter_info, booter_info.comp_cap, thd_fn_mthds_classic, NULL);
    if (EXPECT_LL_LT(1, ts, "Thread Classic: Cannot Allocate")) {
        return;
    }

    for (i = 0; i < ITER; i++) {
        ret = cos_thd_switch(ts);
        if(EXPECT_LL_NEQ(0, ret, "Thread Classic: COS Switch Error")) return;
    }
    CHECK_STATUS_FLAG();
    PRINTC("\t%s: \t\tSuccess\n", "THD => Switch in pairs");
    EXIT_FN();
}

static void
thd_tls(void *d)
{
    if (EXPECT_LLU_NEQ((long unsigned)tls_get(0), (long unsigned)tls_test[cos_cpuid()][(int)d],
                       "Thread TLS: ARG not correct")) failure = 1;
    while (1) cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);
    EXPECT_LL_NEQ(1, 0, "Error, shouldn't get here!\n");
}

                    /* Test the TLS support */
static void
test_thds_tls(void)
{
    thdcap_t ts[TEST_NTHDS];
    intptr_t i;
    int ret;

    for (i = 0; i < TEST_NTHDS; i++) {
        ts[i] = cos_thd_alloc(&booter_info, booter_info.comp_cap, thd_tls, (void *)i);
        if (EXPECT_LL_LT(1, ts[i], "Thread TLS: Cannot Allocate")) {
            return;
        }
        tls_test[cos_cpuid()][i] = i;
        cos_thd_mod(&booter_info, ts[i], &tls_test[cos_cpuid()][i]);
        ret = cos_thd_switch(ts[i]);
        if (EXPECT_LL_NEQ(0, ret, "Thread TLS: COS Switch Error")) return;
    }

    CHECK_STATUS_FLAG();
    PRINTC("\t%s: \t\t\tSuccess\n", "THD => Creation & TLS");
    EXIT_FN();
}

#define TEST_NPAGES (1024 * 2)      /* Testing with 8MB for now */

static void
spinner(void *d)
{
    while (1)
        ;
}

void
sched_events_clear(void)
{
    thdid_t     tid;
    int     blocked, rcvd;
    cycles_t    cycles, now;
    tcap_time_t timer, thd_timeout;

    while (cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, RCV_ALL_PENDING, 0,
                         &rcvd, &tid, &blocked, &cycles, &thd_timeout) != 0)
        ;

}

static void
test_timer(void)
{
    thdcap_t    tc;
    cycles_t    c = 0, p = 0;
    int         i, ret;
    cycles_t    s, e;
    thdid_t     tid;
    int     blocked, rcvd;
    cycles_t    cycles, now, utime;
    long long time, mask;
    tcap_time_t timer, thd_timeout;

    tc = cos_thd_alloc(&booter_info, booter_info.comp_cap, spinner, NULL);

    perfdata_init(&result_test_timer, "COS THD => COS_THD_SWITCH");

    for (i = 0; i <= TEST_ITER; i++){
        rdtscll(now);
        timer = tcap_cyc2time(now + GRANULARITY * cyc_per_usec);
        cos_switch(tc, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, 0, timer, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE,
                   cos_sched_sync());
        p = c;
        rdtscll(c);
        time = (c - now - (cycles_t)(GRANULARITY * cyc_per_usec));
        mask = (time >> (sizeof(long long) * CHAR_BIT - 1));
        utime = (time + mask) ^ mask;

        if (i > 0) {
            perfdata_add(&result_test_timer, utime);

            if (EXPECT_LLU_LT((long long unsigned)(c-now), (unsigned)(GRANULARITY * cyc_per_usec * MAX_THDS),
                            "Timer: Failure on  MAX") ||
                EXPECT_LLU_LT((unsigned)(GRANULARITY * cyc_per_usec * MIN_THDS), (long long unsigned)(c-now),
                            "Timer: failure on MIN")) {
                return;
            }
        }   
        sched_events_clear();
    }

    /* TIMER IN PAST */
    c = 0, p = 0;

    rdtscll(c);
    timer = tcap_cyc2time(c - GRANULARITY * cyc_per_usec);
    cos_switch(tc, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, 0, timer, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE,
               cos_sched_sync());
    p = c;
    rdtscll(c);

    if (EXPECT_LLU_LT((long long unsigned)(c-p), (unsigned)(GRANULARITY * cyc_per_usec), "Timer: Past")) {
        return;
    }

    sched_events_clear();

    /* TIMER NOW */
    c = 0, p = 0;

    rdtscll(c);
    timer = tcap_cyc2time(c);
    cos_switch(tc, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, 0, timer, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE,
               cos_sched_sync());
    p = c;
    rdtscll(c);

    if (EXPECT_LLU_LT((long long unsigned)(c-p), (unsigned)(GRANULARITY * cyc_per_usec), "Timer:  Now")) {
        return;
    }

    cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, RCV_ALL_PENDING, 0,
                  &rcvd, &tid, &blocked, &cycles, &thd_timeout)
            ;

    EXPECT_LLU_LT((long long unsigned)cycles, (long long unsigned)(c-p), "Timer => Cycles time");

    sched_events_clear();
    PRINTC("\t%s: \t\t\tSuccess\n", "One-Shot Timeout");
}

struct exec_cluster {
    thdcap_t    tc;
    arcvcap_t   rc;
    tcap_t      tcc;
    cycles_t    cyc;
    asndcap_t   sc;         /* send-cap to send to rc */
    tcap_prio_t prio;
    int         xseq;       /* expected activation sequence number for this thread */
};

struct budget_test_data {
                    /* p=parent, c=child, g=grand-child */
    struct exec_cluster p, c, g;
} bt[NUM_CPU], mbt[NUM_CPU];

static int
exec_cluster_alloc(struct exec_cluster *e, cos_thd_fn_t fn, void *d, arcvcap_t parentc)
{
    e->tcc = cos_tcap_alloc(&booter_info);
    if (EXPECT_LL_LT(1, e->tcc, "Cluster Allocation: TCAP ALLOC")) return -1;
    e->tc = cos_thd_alloc(&booter_info, booter_info.comp_cap, fn, d);
    if (EXPECT_LL_LT(1, e->tc, "Cluster Allocation: THD ALLOC")) return -1;
    e->rc = cos_arcv_alloc(&booter_info, e->tc, e->tcc, booter_info.comp_cap, parentc);
    if (EXPECT_LL_LT(1, e->rc, "Cluster Allocation: ARCV ALLOC")) return -1;
    e->sc = cos_asnd_alloc(&booter_info, e->rc, booter_info.captbl_cap);
    if (EXPECT_LL_LT(1, e->sc, "Cluster Allocation: ASND ALLOC")) return -1;

    e->cyc = 0;

    return 0;
}

static void
parent(void *d)
{
    assert(0);
}

static void
spinner_cyc(void *d)
{
    cycles_t *p = (cycles_t *)d;

    while (1) rdtscll(*p);
}

#define TIMER_TIME 100

static void
test_2timers(void)
{
    int ret;
    cycles_t    s, e, timer;

    if (EXPECT_LL_NEQ(0, exec_cluster_alloc(&bt[cos_cpuid()].p, parent, &bt[cos_cpuid()].p,
                      BOOT_CAPTBL_SELF_INITRCV_CPU_BASE), "TCAP v. Timer: Cannot Allocate")) {
        return;
    }
    if (EXPECT_LL_NEQ(0, exec_cluster_alloc(&bt[cos_cpuid()].c, spinner, &bt[cos_cpuid()].c,
                      bt[cos_cpuid()].p.rc), "TCAP v. Timer: Cannot Allocate")) {
        return;
    }

                    /* Timer > TCAP */

    ret = cos_tcap_transfer(bt[cos_cpuid()].c.rc, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE,
                            GRANULARITY * TIMER_TIME, TCAP_PRIO_MAX + 2);
    if (EXPECT_LL_NEQ(0, ret, "TCAP v. Timer : TCAP Transfer")) {
        return;
    }

    rdtscll(s);
    timer = tcap_cyc2time(s + GRANULARITY * cyc_per_usec);
    if (cos_switch(bt[cos_cpuid()].c.tc, bt[cos_cpuid()].c.tcc, TCAP_PRIO_MAX + 2,
                   timer, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, cos_sched_sync())) {
        EXPECT_LL_NEQ(0, 1, "TCAP v. Timer: COS Switch");
        return;
    }
    rdtscll(e);

    if (EXPECT_LLU_LT((long long unsigned)(e-s), (unsigned)(GRANULARITY * cyc_per_usec),
                      "TCAP v. Timer: Timer > TCAP") ||
        EXPECT_LLU_LT((unsigned)(GRANULARITY * TIMER_TIME), (long long unsigned)(e-s),
                      "TCAP v. Timer: Interreupt Under")) {
        return;
    }

    sched_events_clear();

                    /* Timer < TCAP */

    ret = cos_tcap_transfer(bt[cos_cpuid()].c.rc, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE,
            GRANULARITY * cyc_per_usec, TCAP_PRIO_MAX + 2);
    if (EXPECT_LL_NEQ(0, ret, "TCAP v. Timer: TCAP Transfer")) {
        return;
    }

    rdtscll(s);
    timer = tcap_cyc2time(s + GRANULARITY * TIMER_TIME);
    if (EXPECT_LL_NEQ(0, cos_switch(bt[cos_cpuid()].c.tc, bt[cos_cpuid()].c.tcc, TCAP_PRIO_MAX + 2, timer,
                      BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, cos_sched_sync()), "TCAP v. TImer: COS Switch")) {
        return;
    }

    rdtscll(e);

    if (EXPECT_LLU_LT((long long unsigned)(e-s), (unsigned)(GRANULARITY * cyc_per_usec),
                      "TCAP v. Timer: Timer < TCAP") ||
        EXPECT_LLU_LT((unsigned)(GRANULARITY * TIMER_TIME), (long long unsigned)(e-s),
                      "TCAP v. Timer: Interreupt Under")) {
        return;
    }

    sched_events_clear();
    PRINTC("\t%s: \t\tSuccess\n", "Timer => Timeout v. Budget");
}

#define BUDGET_TIME 100

static void
test_budgets_single(void)
{
    int i;
    cycles_t    s = 0, e = 0;
    cycles_t    time, mask;
    int         ret;

    perfdata_init(&result_budgets_single, "Timer => Budget based");

    if (EXPECT_LL_NEQ(0, exec_cluster_alloc(&bt[cos_cpuid()].p, parent, &bt[cos_cpuid()].p,
                      BOOT_CAPTBL_SELF_INITRCV_CPU_BASE), "Single Budget: Cannot Allocate") ||
        EXPECT_LL_NEQ(0, exec_cluster_alloc(&bt[cos_cpuid()].c, spinner, &bt[cos_cpuid()].c,
                      bt[cos_cpuid()].p.rc), "Single Budget: Cannot Allocate")) {
        return;
    }
    for (i = 1; i <= TEST_ITER; i++) {

        ret = cos_tcap_transfer(bt[cos_cpuid()].c.rc, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE,
                                GRANULARITY * BUDGET_TIME, TCAP_PRIO_MAX + 2);
        if (EXPECT_LL_NEQ(0, ret, "Single Budget: TCAP Transfer")) {
            return;
        }

        rdtscll(s);
        if (cos_switch(bt[cos_cpuid()].c.tc, bt[cos_cpuid()].c.tcc, TCAP_PRIO_MAX + 2, TCAP_TIME_NIL,
                       BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, cos_sched_sync())){
            EXPECT_LL_NEQ(0, 1, "Single Budget: COS Switch");
            return;
        }
        rdtscll(e);

        if (i > 1) {
                    /* FAST ABS */
            time = (e - s - (GRANULARITY * BUDGET_TIME));
            mask = (time >> (sizeof(cycles_t) * CHAR_BIT - 1));
            time = (time + mask) ^ mask;

            perfdata_add(&result_budgets_single, time);

            if (EXPECT_LLU_LT((long long unsigned)(e-s), (unsigned)(GRANULARITY * BUDGET_TIME * MAX_THDS),
                              "Single Budget: MAX Bound") ||
                EXPECT_LLU_LT((unsigned)(GRANULARITY * BUDGET_TIME * MIN_THDS), (long long unsigned)(e-s),
                              "Single Budget: MIN Bound")) {
                return;
            }
        }
        sched_events_clear();
    }

    PRINTC("\t%s: \t\t\tSuccess\n", "Timer => Budget based");
}

#define RATE_1 1600
#define RATE_2 800

static void
test_budgets_multi(void)
{
    int i;

    if(EXPECT_LL_NEQ(0, exec_cluster_alloc(&mbt[cos_cpuid()].p, spinner_cyc, &(mbt[cos_cpuid()].p.cyc),
                     BOOT_CAPTBL_SELF_INITRCV_CPU_BASE), "Multi Budget: Cannot Allocate") ||
       EXPECT_LL_NEQ(0, exec_cluster_alloc(&mbt[cos_cpuid()].c, spinner_cyc, &(mbt[cos_cpuid()].c.cyc),
                     mbt[cos_cpuid()].p.rc), "Multi Budget: Cannot Allocate") ||
       EXPECT_LL_NEQ(0, exec_cluster_alloc(&mbt[cos_cpuid()].g, spinner_cyc, &(mbt[cos_cpuid()].g.cyc),
                     mbt[cos_cpuid()].c.rc), "Multi Budget: Cannot allocate")) {
        return;
    }

    for (i = 1; i <= TEST_ITER; i++) {
        tcap_res_t  res;
        thdid_t     tid;
        int     blocked;
        cycles_t    cycles, s, e;
        tcap_time_t thd_timeout;

                    /* test both increasing budgets and constant budgets */
        if (i > (TEST_ITER/2))
            res = GRANULARITY * RATE_1;
        else
            res = i * GRANULARITY * RATE_2;

        if (EXPECT_LL_NEQ(0, cos_tcap_transfer(mbt[cos_cpuid()].p.rc, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE,
                          res, TCAP_PRIO_MAX + 2), "Multi Budget: TCAP Transfer") ||
            EXPECT_LL_NEQ(0, cos_tcap_transfer(mbt[cos_cpuid()].c.rc, mbt[cos_cpuid()].p.tcc, res / 2,
                          TCAP_PRIO_MAX + 2), "Multi Budget: TCAP Transfer") ||
            EXPECT_LL_NEQ(0, cos_tcap_transfer(mbt[cos_cpuid()].g.rc, mbt[cos_cpuid()].c.tcc, res / 4,
                          TCAP_PRIO_MAX + 2), "Multi Budget: TCAP Transfer")) {
            return;
        }

        mbt[cos_cpuid()].p.cyc = mbt[cos_cpuid()].c.cyc = mbt[cos_cpuid()].g.cyc = 0;
        rdtscll(s);
        if (cos_switch(mbt[cos_cpuid()].g.tc, mbt[cos_cpuid()].g.tcc, TCAP_PRIO_MAX + 2, TCAP_TIME_NIL,
                       BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, cos_sched_sync())) {
            EXPECT_LL_NEQ(0, 1, "Multi Budget: COS Switch");
            return;
        }
        rdtscll(e);

        cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, 0, 0, NULL, &tid, &blocked, &cycles, &thd_timeout);

        if ( i > 1) {
            if (EXPECT_LLU_LT((mbt[cos_cpuid()].g.cyc - s), (res / 4 * MAX_THDS), "Multi Budget: G") ||
                EXPECT_LLU_LT(mbt[cos_cpuid()].g.cyc - s, res / 4 * MAX_THDS, "Multi Budget: G MAX Bound") ||
                EXPECT_LLU_LT(res / 4 * MIN_THDS, mbt[cos_cpuid()].g.cyc - s, "Multi Budget: G MIN Bound") ||
                EXPECT_LLU_LT(mbt[cos_cpuid()].c.cyc - s, res / 2 * MAX_THDS, "Multi Budget: C MAX Bound") ||
                EXPECT_LLU_LT(res / 2 * MIN_THDS, mbt[cos_cpuid()].c.cyc - s, "Multi Budget: C MIN Bound") ||
                EXPECT_LLU_LT(mbt[cos_cpuid()].p.cyc - s, res * MAX_THDS, "Multi Budget: P MAX Bound")     ||
                EXPECT_LLU_LT(res * MIN_THDS, mbt[cos_cpuid()].p.cyc - s, "Multi Budget: P MIN BOund")) {
                return;
            }
        }
    }
    PRINTC("\t%s: \t\tSuccess\n", "Timer => Hierarchical Budget");
}

static void
test_budgets(void)
{
    /* single-level budgets test */
    test_budgets_single();

    /* multi-level budgets test */
    test_budgets_multi();
}

/* Executed in micro_booter environment */
static void
test_mem_alloc(void)
{
    char *      p, *s, *t, *prev;
    int         i;
    const char *chk = "SUCCESS";
    int         fail_contiguous = 0;

    p = cos_page_bump_alloc(&booter_info);
    if (p == NULL) {
        EXPECT_LL_NEQ(0, 1, "Memory Test: Cannot Allocate");
        return;
    }
    PRINTC("\t%s: \t\t\tSuccess\n", "Memory => Allocation");
    strcpy(p, chk);

    if (EXPECT_LL_NEQ(0, strcmp(chk, p), "Memory Test: Wrong STRCPY")) {
        return;
    }

    s = cos_page_bump_alloc(&booter_info);
    assert(s);
    prev = s;
    for (i = 0; i < TEST_NPAGES; i++) {
        t = cos_page_bump_alloc(&booter_info);
        if (t == NULL){
            EXPECT_LL_EQ(0, 1, "Memory Test: Cannot Allocate");
            return;
        }
        if (t != prev + PAGE_SIZE) {
            fail_contiguous = 1;
        }
        prev = t;
    }
    if (!fail_contiguous) {
        memset(s, 0, TEST_NPAGES * PAGE_SIZE);
    } else if (EXPECT_LL_EQ(i, TEST_NPAGES,"Memory Test: Cannot Allocate contiguous")) {
        return;
    }

    t = cos_page_bump_allocn(&booter_info, TEST_NPAGES * PAGE_SIZE);
    if (t == NULL) {
        EXPECT_LL_NEQ(0, 1, "Memory Test: Cannot Allocate");
        return;
    }
    memset(t, 0, TEST_NPAGES * PAGE_SIZE);
    PRINTC("\t%s: \t\t\tSuccess\n", "Memory => R & W");
}

static volatile arcvcap_t rcc_global[NUM_CPU], rcp_global[NUM_CPU];
static volatile asndcap_t scp_global[NUM_CPU];
static int                async_test_flag_[NUM_CPU] = { 0 };

#define TEST_TIMEOUT_MS 1

static void
async_thd_fn(void *thdcap)
{
    thdcap_t  tc = (thdcap_t)thdcap;
    arcvcap_t rc = rcc_global[cos_cpuid()];
    int       pending, rcvd, ret;

    pending = cos_rcv(rc, RCV_NON_BLOCKING, NULL);
    if (EXPECT_LL_NEQ(3, pending, "Test Async Endpoints")) failure = 1;

    pending = cos_rcv(rc, RCV_NON_BLOCKING | RCV_ALL_PENDING, &rcvd);
    if (EXPECT_LL_NEQ(0, pending, "Test Async Endpoints")) failure = 1;

    pending = cos_rcv(rc, RCV_ALL_PENDING, &rcvd);

    /* switch */
    if (EXPECT_LL_NEQ(0, pending, "Test Async Endpoints")) failure = 1;

    pending = cos_rcv(rc, 0, NULL);

    /* switch */
    if (EXPECT_LL_NEQ(0, pending, "Test Async Endpoints")) failure = 1;

    pending = cos_rcv(rc, 0, NULL);

    /* switch */
    if (EXPECT_LL_NEQ(0, pending, "Test Async Endpoints")) failure = 1;

    pending = cos_rcv(rc, RCV_NON_BLOCKING, NULL);
    if (EXPECT_LL_NEQ(pending, -EAGAIN, "Test Async Endpoints")) failure = 1;

    pending = cos_rcv(rc, 0, NULL);

    /* switch */
    if (EXPECT_LL_NEQ(0, 1, "Test Async Endpoints")) failure = 1;

    ret = cos_thd_switch(tc);
    if (EXPECT_LL_NEQ(0, ret, "COS Switch Error") ||
        EXPECT_LL_NEQ(0, 1, "Test Async Endpoints")) {
        failure = 1;
    }
    while (1) cos_thd_switch(tc);
}

static void
async_thd_parent(void *thdcap)
{
    thdcap_t    tc = (thdcap_t)thdcap;
    arcvcap_t   rc = rcp_global[cos_cpuid()];
    asndcap_t   sc = scp_global[cos_cpuid()];
    int         ret;
    thdid_t     tid;
    int         blocked, rcvd;
    cycles_t    cycles, now;
    tcap_time_t thd_timeout;

    /* NON_BLOCKING ASND with 0 as arg*/

    ret = cos_asnd(sc, 0);
    ret = cos_asnd(sc, 0);
    ret = cos_asnd(sc, 0);
    ret = cos_asnd(sc, 1);

    /* switch */

    /* child blocked at this point, parent is using child's tcap, this call yields to the child */
    ret = cos_asnd(sc, 0);

    /* switch */
    ret = cos_asnd(sc, 0);
    if (EXPECT_LL_NEQ(0, ret, "Test Async Endpoints")) failure = 1;

    /* switch */
    ret = cos_asnd(sc, 1);
    if (EXPECT_LL_NEQ(0, ret, "Test Async Endpoints")) failure = 1;

    /* switch */
    cos_sched_rcv(rc, RCV_ALL_PENDING, 0, &rcvd, &tid, &blocked, &cycles, &thd_timeout);
    rdtscll(now);

    async_test_flag_[cos_cpuid()] = 0;
    while (1) cos_thd_switch(tc);
}

static void
test_async_endpoints(void)
{
    thdcap_t  tcp, tcc;
    tcap_t    tccp, tccc;
    arcvcap_t rcp, rcc;
    asndcap_t scr;

    /* parent rcv capabilities */

    tcp = cos_thd_alloc(&booter_info, booter_info.comp_cap, async_thd_parent,
                        (void *)BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);
    if(EXPECT_LL_LT(1, tcp, "Test Async Endpoints")) {
        return;
    }
    tccp = cos_tcap_alloc(&booter_info);
    if(EXPECT_LL_LT(1, tccp, "Test Async Endpoints")) {
        return;
    }
    rcp = cos_arcv_alloc(&booter_info, tcp, tccp, booter_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE);
    if(EXPECT_LL_LT(1, rcp, "Test Async Endpoints")) {
        return;
    }
    if(EXPECT_LL_NEQ(0,cos_tcap_transfer(rcp, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, TCAP_PRIO_MAX),
                     "Test Async Endpoints")) {
        return;
    }

    /* child rcv capabilities */

    tcc = cos_thd_alloc(&booter_info, booter_info.comp_cap, async_thd_fn, (void *)tcp);
    if(EXPECT_LL_LT(1, tcc, "Test Async Endpoints")) {
        return;
    }
    tccc = cos_tcap_alloc(&booter_info);
    if(EXPECT_LL_LT(1, tccc, "Test Async Endpoints")) {
        return;
    }
    rcc = cos_arcv_alloc(&booter_info, tcc, tccc, booter_info.comp_cap, rcp);
    if(EXPECT_LL_LT(1, rcc, "Test Async Endpoints")) {
        return;
    }
    if(EXPECT_LL_NEQ(0,cos_tcap_transfer(rcc, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF,
                     TCAP_PRIO_MAX + 1), "Test Async Endpoints")) {
        return;
    }

    /* make the snd channel to the child */

    scp_global[cos_cpuid()] = cos_asnd_alloc(&booter_info, rcc, booter_info.captbl_cap);
    if(EXPECT_LL_EQ(0, scp_global[cos_cpuid()], "Test Async Endpoints")) return;
    scr = cos_asnd_alloc(&booter_info, rcp, booter_info.captbl_cap);
    if(EXPECT_LL_EQ(0, scr, "Test Async Endpoints")) return;

    rcc_global[cos_cpuid()] = rcc;
    rcp_global[cos_cpuid()] = rcp;

    async_test_flag_[cos_cpuid()] = 1;
    while (async_test_flag_[cos_cpuid()]) cos_asnd(scr, 1);

    CHECK_STATUS_FLAG();
    PRINTC("\t%s: \t\tSuccess\n", "Asynchronous Endpoints");
    EXIT_FN();
}

int
test_serverfn(int a, int b, int c)
{
    return 0xDEADBEEF;
}

extern void *__inv_test_serverfn(int a, int b, int c);

static inline int
call_cap_mb(u32_t cap_no, int arg1, int arg2, int arg3)
{
    int ret;

    /*
     * Which stack should we use for this invocation?  Simple, use
     * this stack, at the current sp.  This is essentially a
     * function call into another component, with odd calling
     * conventions.
     */
    cap_no = (cap_no + 1) << COS_CAPABILITY_OFFSET;

    __asm__ __volatile__("pushl %%ebp\n\t"
                         "movl %%esp, %%ebp\n\t"
                         "movl %%esp, %%edx\n\t"
                         "movl $1f, %%ecx\n\t"
                         "sysenter\n\t"
                         "1:\n\t"
                         "popl %%ebp"
                         : "=a"(ret)
                         : "a"(cap_no), "b"(arg1), "S"(arg2), "D"(arg3)
                         : "memory", "cc", "ecx", "edx");

    return ret;
}

static void
test_inv(void)
{
    compcap_t    cc;
    sinvcap_t    ic;
    unsigned int r;
    int          i;
    cycles_t     start_cycles = 0LL, end_cycles = 0LL;

    perfdata_init(&result_sinv, "SINV");

    cc = cos_comp_alloc(&booter_info, booter_info.captbl_cap, booter_info.pgtbl_cap, (vaddr_t)NULL);
    if (EXPECT_LL_LT(1, cc, "Invocation: Cannot Allocate")) return;
    ic = cos_sinv_alloc(&booter_info, cc, (vaddr_t)__inv_test_serverfn, 0);
    if (EXPECT_LL_LT(1, ic, "Invocation: Cannot Allocate")) return;

    r = call_cap_mb(ic, 1, 2, 3);
    if (EXPECT_LLU_NEQ(0xDEADBEEF, r, "Test Invocation")) return;

    for (i = 0; i < ITER; i++) {
        rdtscll(start_cycles);
        call_cap_mb(ic, 1, 2, 3);
        rdtscll(end_cycles);

        perfdata_add(&result_sinv, end_cycles - start_cycles);
    }

    CHECK_STATUS_FLAG();
    PRINTC("\t%s: \t\tSuccess\n", "Synchronous Invocations");
    EXIT_FN();
}

#define CAPTBL_ITER 1024

void
test_captbl_expands(void)
{
    int       i;
    compcap_t cc;

    cc = cos_comp_alloc(&booter_info, booter_info.captbl_cap, booter_info.pgtbl_cap, (vaddr_t)NULL);
    assert(cc);
    if (EXPECT_LL_LT(1, cc, "Capability Table Expansion")) {
        return;
    }
    for (i = 0; i < CAPTBL_ITER; i++) {
        sinvcap_t ic;

        ic = cos_sinv_alloc(&booter_info, cc, (vaddr_t)__inv_test_serverfn, 0);
        if(EXPECT_LL_LT(1, ic, "Capability Table: Cannot Allocate")) {
            return;
        }
    }
    PRINTC("\t%s: \t\tSuccess\n", "Capability Table Expansion");
}

void
test_thds(void)
{
    test_thds_create_switch();
    test_thds_tls();
    test_mthds_classic();
    test_mthds_ring();
}

void
test_run_mb(void)
{
    cyc_per_usec = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);
    test_timer();
    test_budgets();
    test_2timers();
    test_thds();
    test_mem_alloc();
    test_async_endpoints();
    test_inv();
    test_captbl_expands();
}

static void
block_vm(void)
{
    int blocked, rcvd;
    cycles_t cycles, now;
    tcap_time_t timeout, thd_timeout;
    thdid_t tid;

    while (cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, RCV_ALL_PENDING | RCV_NON_BLOCKING, 0,
                         &rcvd, &tid, &blocked, &cycles, &thd_timeout) > 0)
            ;

    rdtscll(now);
    now += (1000 * cyc_per_usec);
    timeout = tcap_cyc2time(now);
    cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, RCV_ALL_PENDING, timeout,
                  &rcvd, &tid, &blocked, &cycles, &thd_timeout);
}

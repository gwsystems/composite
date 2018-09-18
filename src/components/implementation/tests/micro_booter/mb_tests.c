#include <stdint.h>

#include "micro_booter.h"

unsigned int cyc_per_usec;

static void
thd_fn_perf(void *d)
{
	cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);

	while (1) {
		cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);
	}
	PRINTC("Error, shouldn't get here!\n");
}

static void
test_thds_perf(void)
{
	thdcap_t  ts;
	long long total_swt_cycles = 0;
	long long start_swt_cycles = 0, end_swt_cycles = 0;
	int       i;

	ts = cos_thd_alloc(&booter_info, booter_info.comp_cap, thd_fn_perf, NULL);
	assert(ts);
	cos_thd_switch(ts);

	rdtscll(start_swt_cycles);
	for (i = 0; i < ITER; i++) {
		cos_thd_switch(ts);
	}
	rdtscll(end_swt_cycles);
	total_swt_cycles = (end_swt_cycles - start_swt_cycles) / 2LL;

	PRINTC("Average THD SWTCH (Total: %lld / Iterations: %lld ): %lld\n", total_swt_cycles, (long long)ITER,
	       (total_swt_cycles / (long long)ITER));
}

static void
thd_fn(void *d)
{
	PRINTC("\tNew thread %d with argument %d, capid %ld\n",
	       cos_thdid(), (int)d, tls_test[cos_cpuid()][(int)d]);
	/* Test the TLS support! */
	assert(tls_get(0) == tls_test[cos_cpuid()][(int)d]);
	while (1) cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);
	PRINTC("Error, shouldn't get here!\n");
}

static void
test_thds(void)
{
	thdcap_t ts[TEST_NTHDS];
	intptr_t i;

	for (i = 0; i < TEST_NTHDS; i++) {
		ts[i] = cos_thd_alloc(&booter_info, booter_info.comp_cap, thd_fn, (void *)i);
		assert(ts[i]);
		tls_test[cos_cpuid()][i] = i;
		cos_thd_mod(&booter_info, ts[i], &tls_test[cos_cpuid()][i]);
		PRINTC("switchto %d\n", (int)ts[i]);
		cos_thd_switch(ts[i]);
	}

	PRINTC("test done\n");
}

#define TEST_NPAGES (1024 * 2) /* Testing with 8MB for now */

static void
test_mem(void)
{
	char *      p, *s, *t, *prev;
	int         i;
	const char *chk = "SUCCESS";
	int fail_contiguous = 0;

	p = cos_page_bump_alloc(&booter_info);
	assert(p);
	strcpy(p, chk);

	assert(0 == strcmp(chk, p));
	PRINTC("%s: Page allocation\n", p);

	s = cos_page_bump_alloc(&booter_info);
	assert(s);
	prev = s;
	for (i = 0; i < TEST_NPAGES; i++) {
		t = cos_page_bump_alloc(&booter_info);
		assert(t);
		if (t != prev + PAGE_SIZE) {
			fail_contiguous = 1;
		}
		prev = t;
	}
	if (!fail_contiguous) {
		memset(s, 0, TEST_NPAGES * PAGE_SIZE);
		PRINTC("SUCCESS: Allocated and zeroed %d contiguous pages.\n", TEST_NPAGES);
	} else if (i == TEST_NPAGES) {
		PRINTC("FAILURE: Cannot allocate contiguous %d pages.\n", TEST_NPAGES);
	}

	t = cos_page_bump_allocn(&booter_info, TEST_NPAGES * PAGE_SIZE);
	assert(t);
	memset(t, 0, TEST_NPAGES * PAGE_SIZE);
	PRINTC("SUCCESS: Atomically allocated and zeroed %d contiguous pages.\n", TEST_NPAGES);
}

volatile arcvcap_t rcc_global[NUM_CPU], rcp_global[NUM_CPU];
volatile asndcap_t scp_global[NUM_CPU];
int                async_test_flag[NUM_CPU] = { 0 };

static void
async_thd_fn_perf(void *thdcap)
{
	thdcap_t  tc = (thdcap_t)thdcap;
	arcvcap_t rc = rcc_global[cos_cpuid()];
	int       i;

	cos_rcv(rc, 0, NULL);

	for (i = 0; i < ITER + 1; i++) {
		cos_rcv(rc, 0, NULL);
	}

	cos_thd_switch(tc);
}

static void
async_thd_parent_perf(void *thdcap)
{
	thdcap_t  tc                = (thdcap_t)thdcap;
	asndcap_t sc                = scp_global[cos_cpuid()];
	long long total_asnd_cycles = 0;
	long long start_asnd_cycles = 0, end_arcv_cycles = 0;
	int       i;

	cos_asnd(sc, 1);

	rdtscll(start_asnd_cycles);
	for (i = 0; i < ITER; i++) {
		cos_asnd(sc, 1);
	}
	rdtscll(end_arcv_cycles);
	total_asnd_cycles = (end_arcv_cycles - start_asnd_cycles) / 2;

	PRINTC("Average ASND/ARCV (Total: %lld / Iterations: %lld ): %lld\n", total_asnd_cycles, (long long)(ITER),
	       (total_asnd_cycles / (long long)(ITER)));

	async_test_flag[cos_cpuid()] = 0;
	while (1) cos_thd_switch(tc);
}

#define TEST_TIMEOUT_MS 1

static void
async_thd_fn(void *thdcap)
{
	thdcap_t  tc = (thdcap_t)thdcap;
	arcvcap_t rc = rcc_global[cos_cpuid()];
	int       pending, rcvd;

	PRINTC("Asynchronous event thread handler.\n");
	PRINTC("<-- rcving (non-blocking)...\n");
	pending = cos_rcv(rc, RCV_NON_BLOCKING, NULL);
	PRINTC("<-- pending %d\n", pending);

	PRINTC("<-- rcving (non-blocking & all pending)...\n");
	pending = cos_rcv(rc, RCV_NON_BLOCKING | RCV_ALL_PENDING, &rcvd);
	PRINTC("<-- rcvd %d\n", rcvd);

	PRINTC("<-- rcving (all pending)...\n");
	pending = cos_rcv(rc, RCV_ALL_PENDING, &rcvd);
	PRINTC("<-- rcvd %d\n", rcvd);

	PRINTC("<-- rcving...\n");
	pending = cos_rcv(rc, 0, NULL);
	PRINTC("<-- pending %d\n", pending);
	PRINTC("<-- rcving...\n");
	pending = cos_rcv(rc, 0, NULL);
	PRINTC("<-- pending %d\n", pending);

	PRINTC("<-- rcving (non-blocking)...\n");
	pending = cos_rcv(rc, RCV_NON_BLOCKING, NULL);
	PRINTC("<-- pending %d\n", pending);
	assert(pending == -EAGAIN);
	PRINTC("<-- rcving\n");

	pending = cos_rcv(rc, 0, NULL);
	PRINTC("<-- Error: manually returning to snding thread.\n");

	cos_thd_switch(tc);
	PRINTC("ERROR: in async thd *after* switching back to the snder.\n");
	while (1) cos_thd_switch(tc);
}

static void
async_thd_parent(void *thdcap)
{
	thdcap_t    tc = (thdcap_t)thdcap;
	arcvcap_t   rc = rcp_global[cos_cpuid()];
	asndcap_t   sc = scp_global[cos_cpuid()];
	int         ret, pending;
	thdid_t     tid;
	int         blocked, rcvd;
	cycles_t    cycles, now;
	tcap_time_t thd_timeout;

	PRINTC("--> sending\n");
	ret = cos_asnd(sc, 0);
	PRINTC("--> sending\n");
	ret = cos_asnd(sc, 0);
	PRINTC("--> sending\n");
	ret = cos_asnd(sc, 0);
	PRINTC("--> sending\n");
	ret = cos_asnd(sc, 1);

	PRINTC("--> sending\n");
	/* child blocked at this point, parent is using child's tcap, this call yields to the child */
	ret = cos_asnd(sc, 0);

	PRINTC("--> sending\n");
	ret = cos_asnd(sc, 0);
	if (ret) PRINTC("asnd returned %d.\n", ret);
	PRINTC("--> Back in the asnder.\n");
	PRINTC("--> sending\n");
	ret = cos_asnd(sc, 1);
	if (ret) PRINTC("--> asnd returned %d.\n", ret);

	PRINTC("--> Back in the asnder.\n");
	PRINTC("--> receiving to get notifications\n");
	pending = cos_sched_rcv(rc, RCV_ALL_PENDING, 0, &rcvd, &tid, &blocked, &cycles, &thd_timeout);
	rdtscll(now);
	PRINTC("--> pending %d, thdid %d, blocked %d, cycles %lld, timeout %lu (now=%llu, abs:%llu)\n",
	       pending, tid, blocked, cycles, thd_timeout, now, tcap_time2cyc(thd_timeout, now));

	async_test_flag[cos_cpuid()] = 0;
	while (1) cos_thd_switch(tc);
}

static void
test_async_endpoints(void)
{
	thdcap_t  tcp, tcc;
	tcap_t    tccp, tccc;
	arcvcap_t rcp, rcc;
	asndcap_t scr;
	int       ret;

	PRINTC("Creating threads, and async end-points.\n");
	/* parent rcv capabilities */
	tcp = cos_thd_alloc(&booter_info, booter_info.comp_cap, async_thd_parent,
	                    (void *)BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);
	assert(tcp);
	tccp = cos_tcap_alloc(&booter_info);
	assert(tccp);
	rcp = cos_arcv_alloc(&booter_info, tcp, tccp, booter_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE);
	assert(rcp);
	if ((ret = cos_tcap_transfer(rcp, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, TCAP_PRIO_MAX))) {
		PRINTC("transfer failed: %d\n", ret);
		assert(0);
	}

	/* child rcv capabilities */
	tcc = cos_thd_alloc(&booter_info, booter_info.comp_cap, async_thd_fn, (void *)tcp);
	assert(tcc);
	tccc = cos_tcap_alloc(&booter_info);
	assert(tccc);
	rcc = cos_arcv_alloc(&booter_info, tcc, tccc, booter_info.comp_cap, rcp);
	assert(rcc);
	if (cos_tcap_transfer(rcc, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, TCAP_PRIO_MAX + 1)) assert(0);

	/* make the snd channel to the child */
	scp_global[cos_cpuid()] = cos_asnd_alloc(&booter_info, rcc, booter_info.captbl_cap);
	assert(scp_global[cos_cpuid()]);
	scr = cos_asnd_alloc(&booter_info, rcp, booter_info.captbl_cap);
	assert(scr);

	rcc_global[cos_cpuid()] = rcc;
	rcp_global[cos_cpuid()] = rcp;

	async_test_flag[cos_cpuid()] = 1;
	while (async_test_flag[cos_cpuid()]) cos_asnd(scr, 1);

	PRINTC("Async end-point test successful.\n");
	PRINTC("Test done.\n");
}

static void
test_async_endpoints_perf(void)
{
	thdcap_t  tcp, tcc;
	tcap_t    tccp, tccc;
	arcvcap_t rcp, rcc;

	/* parent rcv capabilities */
	tcp = cos_thd_alloc(&booter_info, booter_info.comp_cap, async_thd_parent_perf,
	                    (void *)BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);
	assert(tcp);
	tccp = cos_tcap_alloc(&booter_info);
	assert(tccp);
	rcp = cos_arcv_alloc(&booter_info, tcp, tccp, booter_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE);
	assert(rcp);
	if (cos_tcap_transfer(rcp, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, TCAP_PRIO_MAX + 1)) assert(0);

	/* child rcv capabilities */
	tcc = cos_thd_alloc(&booter_info, booter_info.comp_cap, async_thd_fn_perf, (void *)tcp);
	assert(tcc);
	tccc = cos_tcap_alloc(&booter_info);
	assert(tccc);
	rcc = cos_arcv_alloc(&booter_info, tcc, tccc, booter_info.comp_cap, rcp);
	assert(rcc);
	if (cos_tcap_transfer(rcc, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, TCAP_PRIO_MAX)) assert(0);

	/* make the snd channel to the child */
	scp_global[cos_cpuid()] = cos_asnd_alloc(&booter_info, rcc, booter_info.captbl_cap);
	assert(scp_global[cos_cpuid()]);

	rcc_global[cos_cpuid()] = rcc;
	rcp_global[cos_cpuid()] = rcp;

	async_test_flag[cos_cpuid()] = 1;
	while (async_test_flag[cos_cpuid()]) cos_thd_switch(tcp);
}

static void
spinner(void *d)
{
	while (1)
		;
}

static void
test_timer(void)
{
	int      i;
	thdcap_t tc;
	cycles_t c = 0, p = 0, t = 0;

	PRINTC("Starting timer test.\n");
	tc = cos_thd_alloc(&booter_info, booter_info.comp_cap, spinner, NULL);

	for (i = 0; i <= 16; i++) {
		thdid_t     tid;
		int         blocked, rcvd;
		cycles_t    cycles, now;
		tcap_time_t timer, thd_timeout;

		rdtscll(now);
		timer = tcap_cyc2time(now + 1000 * cyc_per_usec);
		cos_switch(tc, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, 0, timer, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE,
		           cos_sched_sync());
		p = c;
		rdtscll(c);
		if (i > 0) t += c - p;

		while (cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, RCV_ALL_PENDING, 0,
				     &rcvd, &tid, &blocked, &cycles, &thd_timeout) != 0)
			;
	}

	PRINTC("\tCycles per tick (1000 microseconds) = %lld, cycles threshold = %u\n", t / 16,
	       (unsigned int)cos_hw_cycles_thresh(BOOT_CAPTBL_SELF_INITHW_BASE));

	PRINTC("Timer test completed. Success.\n");
}

struct exec_cluster {
	thdcap_t    tc;
	arcvcap_t   rc;
	tcap_t      tcc;
	cycles_t    cyc;
	asndcap_t   sc; /*send-cap to send to rc */
	tcap_prio_t prio;
	int         xseq; /* expected activation sequence number for this thread */
};

struct budget_test_data {
	/* p=parent, c=child, g=grand-child */
	struct exec_cluster p, c, g;
} bt[NUM_CPU], mbt[NUM_CPU];

static void
exec_cluster_alloc(struct exec_cluster *e, cos_thd_fn_t fn, void *d, arcvcap_t parentc)
{
	e->tcc = cos_tcap_alloc(&booter_info);
	assert(e->tcc);
	e->tc = cos_thd_alloc(&booter_info, booter_info.comp_cap, fn, d);
	assert(e->tc);
	e->rc = cos_arcv_alloc(&booter_info, e->tc, e->tcc, booter_info.comp_cap, parentc);
	assert(e->rc);
	e->sc = cos_asnd_alloc(&booter_info, e->rc, booter_info.captbl_cap);
	assert(e->sc);

	e->cyc = 0;
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

static void
test_budgets_single(void)
{
	int i;

	PRINTC("Starting single-level budget test.\n");

	exec_cluster_alloc(&bt[cos_cpuid()].p, parent, &bt[cos_cpuid()].p, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE);
	exec_cluster_alloc(&bt[cos_cpuid()].c, spinner, &bt[cos_cpuid()].c, bt[cos_cpuid()].p.rc);

	PRINTC("Budget switch latencies: ");
	for (i = 1; i < 10; i++) {
		cycles_t    s, e;
		thdid_t     tid;
		int         blocked, ret;
		cycles_t    cycles;
		tcap_time_t thd_timeout;

		ret = cos_tcap_transfer(bt[cos_cpuid()].c.rc, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, i * 100000, TCAP_PRIO_MAX + 2);
		if (ret) assert(0);

		rdtscll(s);
		if (cos_switch(bt[cos_cpuid()].c.tc, bt[cos_cpuid()].c.tcc, TCAP_PRIO_MAX + 2, TCAP_TIME_NIL, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE,
		               cos_sched_sync()))
			assert(0);
		rdtscll(e);
		PRINTC("\t%lld\n", e - s);

		/* FIXME: we should avoid calling this two times in the common case, return "more evts" */
		while (cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, 0, 0, NULL, &tid, &blocked, &cycles, &thd_timeout) != 0)
			;
	}
	PRINTC("Done.\n");
}

static void
test_budgets_multi(void)
{
	int i;

	PRINTC("Starting multi-level budget test.\n");

	exec_cluster_alloc(&mbt[cos_cpuid()].p, spinner_cyc, &(mbt[cos_cpuid()].p.cyc), BOOT_CAPTBL_SELF_INITRCV_CPU_BASE);
	exec_cluster_alloc(&mbt[cos_cpuid()].c, spinner_cyc, &(mbt[cos_cpuid()].c.cyc), mbt[cos_cpuid()].p.rc);
	exec_cluster_alloc(&mbt[cos_cpuid()].g, spinner_cyc, &(mbt[cos_cpuid()].g.cyc), mbt[cos_cpuid()].c.rc);

	PRINTC("Budget switch latencies:\n");
	for (i = 1; i < 10; i++) {
		tcap_res_t  res;
		thdid_t     tid;
		int         blocked;
		cycles_t    cycles, s, e;
		tcap_time_t thd_timeout;

		/* test both increasing budgets and constant budgets */
		if (i > 5)
			res = 1600000;
		else
			res = i * 800000;

		if (cos_tcap_transfer(mbt[cos_cpuid()].p.rc, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, res, TCAP_PRIO_MAX + 2)) assert(0);
		if (cos_tcap_transfer(mbt[cos_cpuid()].c.rc, mbt[cos_cpuid()].p.tcc, res / 2, TCAP_PRIO_MAX + 2)) assert(0);
		if (cos_tcap_transfer(mbt[cos_cpuid()].g.rc, mbt[cos_cpuid()].c.tcc, res / 4, TCAP_PRIO_MAX + 2)) assert(0);

		mbt[cos_cpuid()].p.cyc = mbt[cos_cpuid()].c.cyc = mbt[cos_cpuid()].g.cyc = 0;
		rdtscll(s);
		if (cos_switch(mbt[cos_cpuid()].g.tc, mbt[cos_cpuid()].g.tcc, TCAP_PRIO_MAX + 2, TCAP_TIME_NIL, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE,
		               cos_sched_sync()))
			assert(0);
		rdtscll(e);

		cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, 0, 0, NULL, &tid, &blocked, &cycles, &thd_timeout);
		PRINTC("g:%llu c:%llu p:%llu => %llu, %d=%llu\n", mbt[cos_cpuid()].g.cyc - s, mbt[cos_cpuid()].c.cyc - s,
		       mbt[cos_cpuid()].p.cyc - s, e - s, tid, cycles);
	}
	PRINTC("Done.\n");
}

static void
test_budgets(void)
{
	/* single-level budgets test */
	test_budgets_single();

	/* multi-level budgets test */
	test_budgets_multi();
}

#define TEST_PRIO_HIGH (TCAP_PRIO_MAX)
#define TEST_PRIO_MED (TCAP_PRIO_MAX + 1)
#define TEST_PRIO_LOW (TCAP_PRIO_MAX + 2)
#define TEST_WAKEUP_BUDGET 400000

struct activation_test_data {
	/* p = preempted, s = scheduler, i = interrupt, w = worker */
	struct exec_cluster p, s, i, w;
} wat[NUM_CPU], pat[NUM_CPU];

int wakeup_test_start[NUM_CPU]  = { 0 };
int wakeup_budget_test[NUM_CPU] = { 0 };
int active_seq[NUM_CPU]         = { 0 };
int final_seq[NUM_CPU]          = { 0 };

static void
seq_expected_order_set(struct exec_cluster *e, int seq)
{
	e->xseq = seq;
}

static void
seq_order_check(struct exec_cluster *e)
{
	assert(e->xseq >= 0);
	assert(e->xseq == active_seq[cos_cpuid()]);

	active_seq[cos_cpuid()]++;
}

/* worker thread thats awoken by intr_thd */
static void
worker_thd(void *d)
{
	struct exec_cluster *e = &(((struct activation_test_data *)d)->w);

	while (1) {
		seq_order_check(e);
		if (wakeup_budget_test[cos_cpuid()]) {
			rdtscll(e->cyc);
			while (1)
				;
		} else {
			cos_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, 0, 0, 0, 0);
		}
	}
}

/* intr thread - interrupt thread */
static void
intr_thd(void *d)
{
	struct exec_cluster *e = &(((struct activation_test_data *)d)->i);
	struct exec_cluster *w = &(((struct activation_test_data *)d)->w);

	while (1) {
		cos_rcv(e->rc, 0, NULL);
		seq_order_check(e);
		cos_thd_wakeup(w->tc, w->tcc, w->prio, wakeup_budget_test[cos_cpuid()] ? TEST_WAKEUP_BUDGET : 0);
	}
}

/* scheduler of the intr_thd */
static void
intr_sched_thd(void *d)
{
	struct exec_cluster *e = &(((struct activation_test_data *)d)->s);
	cycles_t             cycs;
	int                  blocked;
	thdid_t              tid;
	tcap_time_t          thd_timeout;

	while (1) {
		cos_sched_rcv(e->rc, 0, 0, NULL, &tid, &blocked, &cycs, &thd_timeout);
		seq_order_check(e);
		if (wakeup_budget_test[cos_cpuid()]) {
			struct exec_cluster *w = &(((struct activation_test_data *)d)->w);
			rdtscll(e->cyc);
			printc(" | preempted worker @ %llu, budget: %lu=%lu |", e->cyc,
			       (unsigned long)TEST_WAKEUP_BUDGET, (unsigned long)(e->cyc - w->cyc));
		}
	}
}

/* this is preempted thread because, send with yield adds it to preempted */
static void
preempted_thd(void *d)
{
	struct exec_cluster *e = &(((struct activation_test_data *)d)->p);
	struct exec_cluster *i = &(((struct activation_test_data *)d)->i);

	while (1) {
		if (wakeup_test_start[cos_cpuid()]) wakeup_test_start[cos_cpuid()] = 0;

		cos_asnd(i->sc, 1);

		if (!wakeup_test_start[cos_cpuid()]) {
			seq_order_check(e);
			cos_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, 0, 0, 0, 0);
		}
	}
}

static void
test_wakeup_case(struct activation_test_data *at, tcap_prio_t pprio, tcap_prio_t iprio, tcap_prio_t wprio, int pseq,
                 int iseq, int wseq)
{
	active_seq[cos_cpuid()] = 0;
	at->i.prio = iprio;
	seq_expected_order_set(&at->i, iseq);
	at->w.prio = wprio;
	seq_expected_order_set(&at->w, wseq);
	at->p.prio = pprio;
	seq_expected_order_set(&at->p, pseq);

	if (cos_tcap_transfer(at->p.rc, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, at->p.prio)) assert(0);
	if (cos_tcap_transfer(at->i.rc, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, at->i.prio)) assert(0);
	if (cos_tcap_transfer(at->w.rc, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, at->w.prio)) assert(0);
	wakeup_test_start[cos_cpuid()] = 1;
	cos_switch(at->p.tc, at->p.tcc, at->p.prio, TCAP_TIME_NIL, 0, 0);

	assert(active_seq[cos_cpuid()] == final_seq[cos_cpuid()]);
	PRINTC(" - SUCCESS.\n");
}

static void
test_wakeup(void)
{
	PRINTC("Testing Wakeup\n");
	exec_cluster_alloc(&wat[cos_cpuid()].s, intr_sched_thd, &wat[cos_cpuid()], BOOT_CAPTBL_SELF_INITRCV_CPU_BASE);
	exec_cluster_alloc(&wat[cos_cpuid()].p, preempted_thd, &wat[cos_cpuid()], BOOT_CAPTBL_SELF_INITRCV_CPU_BASE);
	exec_cluster_alloc(&wat[cos_cpuid()].i, intr_thd, &wat[cos_cpuid()], wat[cos_cpuid()].s.rc);
	exec_cluster_alloc(&wat[cos_cpuid()].w, worker_thd, &wat[cos_cpuid()], wat[cos_cpuid()].s.rc);
	wat[cos_cpuid()].s.prio = TEST_PRIO_HIGH;        /* scheduler's prio doesn't matter */
	seq_expected_order_set(&wat[cos_cpuid()].s, -1); /* scheduler should not be activated */
	if (cos_tcap_transfer(wat[cos_cpuid()].s.rc, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, wat[cos_cpuid()].s.prio)) assert(0);
	final_seq[cos_cpuid()] = 2;

	/*
	 * test case 1: intr_thd = H, worker_thd = M, preempted_thd = L
	 * expected result:
	 * - cos_thd_wakeup should invalidate preempted_thd and add worker_thd there.
	 * - cos_rcv from intr_thd should activate worker_thd.
	 */
	PRINTC(" Test - Intr = H, Worker = M, Preempted = L");
	test_wakeup_case(&wat[cos_cpuid()], TEST_PRIO_LOW, TEST_PRIO_HIGH, TEST_PRIO_MED, -1, 0, 1);

	/*
	 * test case 2: worker_thd = H, intr_thd = M, preempted_thd = L
	 * expected result:
	 * - cos_thd_wakeup should invalidate preempted_thd and add worker_thd there.
	 * - cos_rcv from intr_thd should activate worker_thd.
	 */
	PRINTC(" Test - Worker = H, Intr = M, Preempted = L");
	test_wakeup_case(&wat[cos_cpuid()], TEST_PRIO_LOW, TEST_PRIO_MED, TEST_PRIO_HIGH, -1, 0, 1);

	/*
	 * test case 3: intr_thd = H, preempted_thd = M, worker_thd = L
	 * expected result:
	 * - cos_thd_wakeup should not invalidate preempted_thd.
	 * - cos_rcv from intr_thd should activate preempted_thd.
	 */
	PRINTC(" Test - Intr = H, Preempted = M, Worker = L");
	test_wakeup_case(&wat[cos_cpuid()], TEST_PRIO_MED, TEST_PRIO_HIGH, TEST_PRIO_LOW, 1, 0, -1);

	/*
	 * test case 4: intr_thd = H, worker_thd = M, preempted_thd = L
	 * 		(+ budget with cos_thd_wakeup)
	 * expected result:
	 * - cos_thd_wakeup should invalidate preempted_thd and add worker_thd there.
	 * - cos_rcv from intr_thd should activate worker_thd.
	 * - worker thread should be preempted after (budget==timeout) and that should activate it's scheduler.
	 */
	PRINTC(" Test Wakeup with Budget -");
	wakeup_budget_test[cos_cpuid()] = 1;
	seq_expected_order_set(&wat[cos_cpuid()].s, 2); /* scheduler should be activated for worker timeout */
	final_seq[cos_cpuid()] = 3;
	test_wakeup_case(&wat[cos_cpuid()], TEST_PRIO_LOW, TEST_PRIO_HIGH, TEST_PRIO_MED, -1, 0, 1);

	wakeup_test_start[cos_cpuid()] = 0;
	PRINTC("Done.\n");
}

static void
receiver_thd(void *d)
{
	struct exec_cluster *e = &(((struct activation_test_data *)d)->w);

	while (1) {
		cos_rcv(e->rc, 0, NULL);
		seq_order_check(e);
	}
}

static void
sender_thd(void *d)
{
	struct exec_cluster *e = &(((struct activation_test_data *)d)->i);
	struct exec_cluster *r = &(((struct activation_test_data *)d)->w);

	while (1) {
		cos_asnd(r->sc, 0);
		seq_order_check(e);
		cos_rcv(e->rc, 0, NULL);
	}
}

static void
test_preemption_case(struct activation_test_data *at, tcap_prio_t iprio, tcap_prio_t wprio, int iseq, int wseq)
{
	active_seq[cos_cpuid()] = 0;
	final_seq[cos_cpuid()]  = 2;
	at->i.prio = iprio;
	seq_expected_order_set(&at->i, iseq);
	at->w.prio = wprio;
	seq_expected_order_set(&at->w, wseq);

	if (cos_tcap_transfer(at->i.rc, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, at->i.prio)) assert(0);
	if (cos_tcap_transfer(at->w.rc, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, at->w.prio)) assert(0);
	cos_switch(at->i.tc, at->i.tcc, at->i.prio, TCAP_TIME_NIL, 0, 0);

	assert(active_seq[cos_cpuid()] == final_seq[cos_cpuid()]);
	PRINTC(" - SUCCESS.\n");
}

static void
test_preemption(void)
{
	PRINTC("Testing Preemption\n");
	exec_cluster_alloc(&pat[cos_cpuid()].i, sender_thd, &pat[cos_cpuid()], BOOT_CAPTBL_SELF_INITRCV_CPU_BASE);
	exec_cluster_alloc(&pat[cos_cpuid()].w, receiver_thd, &pat[cos_cpuid()], BOOT_CAPTBL_SELF_INITRCV_CPU_BASE);

	/*
	 * test case 1: Sender = H, Receiver: L
	 * - scheduler here is initthd (this thd)
	 * expected result:
	 * - cos_asnd from sender should add receiver as wakeup thread.
	 * - cos_rcv from sender should activate receiver thread.
	 */
	PRINTC(" Test - Sender = H, Receiver = L");
	test_preemption_case(&pat[cos_cpuid()], TEST_PRIO_HIGH, TEST_PRIO_LOW, 0, 1);

	/*
	 * test case 2: Sender = L, Receiver: H
	 * - scheduler here is initthd (this thd)
	 * expected result:
	 * - cos_asnd from sender should trigger receiver activation and add sender as wakeup thread.
	 * - cos_rcv from receiver should activate sender thread.
	 */
	PRINTC(" Test - Sender = L, Receiver = H");
	test_preemption_case(&pat[cos_cpuid()], TEST_PRIO_LOW, TEST_PRIO_HIGH, 1, 0);

	PRINTC("Done.\n");
}

long long midinv_cycles[NUM_CPU] = { 0LL };

int
test_serverfn(int a, int b, int c)
{
	rdtscll(midinv_cycles[cos_cpuid()]);
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

	cc = cos_comp_alloc(&booter_info, booter_info.captbl_cap, booter_info.pgtbl_cap, (vaddr_t)NULL);
	assert(cc > 0);
	ic = cos_sinv_alloc(&booter_info, cc, (vaddr_t)__inv_test_serverfn, 0);
	assert(ic > 0);

	r = call_cap_mb(ic, 1, 2, 3);
	PRINTC("Return from invocation: %x (== DEADBEEF?)\n", r);
	PRINTC("Test done.\n");
}

static void
test_inv_perf(void)
{
	compcap_t    cc;
	sinvcap_t    ic;
	int          i;
	long long    total_inv_cycles = 0LL, total_ret_cycles = 0LL;
	unsigned int ret;

	cc = cos_comp_alloc(&booter_info, booter_info.captbl_cap, booter_info.pgtbl_cap, (vaddr_t)NULL);
	assert(cc > 0);
	ic = cos_sinv_alloc(&booter_info, cc, (vaddr_t)__inv_test_serverfn, 0);
	assert(ic > 0);
	ret = call_cap_mb(ic, 1, 2, 3);
	assert(ret == 0xDEADBEEF);

	for (i = 0; i < ITER; i++) {
		long long start_cycles = 0LL, end_cycles = 0LL;

		midinv_cycles[cos_cpuid()] = 0LL;
		rdtscll(start_cycles);
		call_cap_mb(ic, 1, 2, 3);
		rdtscll(end_cycles);
		total_inv_cycles += (midinv_cycles[cos_cpuid()] - start_cycles);
		total_ret_cycles += (end_cycles - midinv_cycles[cos_cpuid()]);
	}

	PRINTC("Average SINV (Total: %lld / Iterations: %lld ): %lld\n", total_inv_cycles, (long long)(ITER),
	       (total_inv_cycles / (long long)(ITER)));
	PRINTC("Average SRET (Total: %lld / Iterations: %lld ): %lld\n", total_ret_cycles, (long long)(ITER),
	       (total_ret_cycles / (long long)(ITER)));
}

void
test_captbl_expand(void)
{
	int       i;
	compcap_t cc;

	cc = cos_comp_alloc(&booter_info, booter_info.captbl_cap, booter_info.pgtbl_cap, (vaddr_t)NULL);
	assert(cc);
	for (i = 0; i < 1024; i++) {
		sinvcap_t ic;

		ic = cos_sinv_alloc(&booter_info, cc, (vaddr_t)__inv_test_serverfn, 0);
		assert(ic > 0);
	}
	PRINTC("Captbl expand SUCCESS.\n");
}

volatile asndcap_t ipi_initasnd_global[NUM_CPU][NUM_CPU];

static void
test_ipi()
{
	arcvcap_t arcv;
	asndcap_t asnd;
	int       ret = 0;
	int	  i;

	if (NUM_CPU == 1) return;

#if 1
	test_ipi_full();
#else
	PRINTC("Creating asnd_cap for IPI test.\n");
	for (i = 0; i < NUM_CPU; i++) {
		asndcap_t snd;

		if (i == cos_cpuid()) continue;

		snd = cos_asnd_alloc(&booter_info, BOOT_CAPTBL_SELF_INITRCV_BASE_CPU(i), booter_info.captbl_cap);
		assert(snd);
		ipi_initasnd_global[cos_cpuid()][i] = snd;
	}

	PRINTC("Sending remote asnd\n");
	for (i = 0; i < NUM_CPU; i++) {

		if (i == cos_cpuid()) continue;

		cos_asnd(ipi_initasnd_global[cos_cpuid()][i], 0);
	}
	PRINTC("Rcving from remote asnd.\n");
	while (ret < NUM_CPU - 1) {
		int rcvd = 0;

		cos_rcv(BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, RCV_ALL_PENDING, &rcvd);
		ret += rcvd;
	}
	PRINTC("Remote Async end-point test successful.\n");
#endif

	return;
}

/* Executed in micro_booter environment */
void
test_run_mb(void)
{
	cyc_per_usec = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);

	/* test_ipi(); */
	test_timer();
	test_budgets();

	test_thds();
	test_thds_perf();

	test_mem();

	test_async_endpoints();
	test_async_endpoints_perf();

	test_inv();
	test_inv_perf();

	test_captbl_expand();

	/*
	 * FIXME: Preemption stack mechanism in the kernel is disabled.
	 * test_wakeup();
	 * test_preemption();
	 */
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
	cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, RCV_ALL_PENDING, timeout, &rcvd, &tid, &blocked, &cycles, &thd_timeout);
}

/*
 * Executed in vkernel environment:
 * Budget tests, tests that use tcaps are not quite feasible in vkernel environment.
 * These tests are designed for micro-benchmarking, and are not intelligent enough to
 * account for VM timeslice, they should not be either.
 */
void
test_run_vk(void)
{
	cyc_per_usec = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);

	test_thds();
	test_thds_perf();
	block_vm();

	test_mem();

	test_inv();
	test_inv_perf();
	block_vm();

	test_captbl_expand();
}

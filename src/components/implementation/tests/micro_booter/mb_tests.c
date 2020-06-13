#include <stdint.h>
#include "sqlite3.h"
#include "duktape.h"

#include "micro_booter.h"

unsigned int cyc_per_usec;

static void
thd_fn_perf(void *d)
{
	cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE);

	while (1) { cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_CPU_BASE); }
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
	for (i = 0; i < ITER; i++) { cos_thd_switch(ts); }
	rdtscll(end_swt_cycles);
	total_swt_cycles = (end_swt_cycles - start_swt_cycles) / 2LL;

	PRINTC("Average THD SWTCH (Total: %lld / Iterations: %lld ): %lld\n", total_swt_cycles, (long long)ITER,
	       (total_swt_cycles / (long long)ITER));
}

static void
thd_fn(void *d)
{
	PRINTC("\tNew thread %d with argument %d, capid %ld\n", cos_thdid(), (int)d,
	       /* tls_test[cos_cpuid()][(int)d] */ 0);
	/* Test the TLS support! */
	// assert(tls_get(0) == tls_test[cos_cpuid()][(int)d]);
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
		// tls_test[cos_cpuid()][i] = i;
		// cos_thd_mod(&booter_info, ts[i], &tls_test[cos_cpuid()][i]);
		PRINTC("switchto %d\n", (int)ts[i]);
		cos_thd_switch(ts[i]);
	}

	PRINTC("test done\n");
}

#define TEST_NPAGES (1024) /* Testing with 4MB for now */

static void
test_mem(void)
{
	char *      p, *s, *t, *prev;
	int         i;
	const char *chk             = "SUCCESS";
	int         fail_contiguous = 0;

	p = cos_page_bump_alloc(&booter_info);
	assert(p);
	printc("omg... success %x\n", p);
	strcpy(p, chk);

	assert(0 == strcmp(chk, p));
	PRINTC("%s: Page allocation\n", p);

	s = cos_page_bump_alloc(&booter_info);
	assert(s);
	prev = s;
	for (i = 0; i < TEST_NPAGES; i++) {
		t = cos_page_bump_alloc(&booter_info);
		assert(t);
		if (t != prev + PAGE_SIZE) { fail_contiguous = 1; }
		prev = t;
	}
	printc("XXXXXXXXXX\n");
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
int                async_test_flag[NUM_CPU] = {0};

static void
async_thd_fn_perf(void *thdcap)
{
	thdcap_t  tc = (thdcap_t)thdcap;
	arcvcap_t rc = rcc_global[cos_cpuid()];
	int       i;

	cos_rcv(rc, 0, NULL);

	for (i = 0; i < ITER + 1; i++) { cos_rcv(rc, 0, NULL); }

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
	for (i = 0; i < ITER; i++) { cos_asnd(sc, 1); }
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
	PRINTC("--> pending %d, thdid %d, blocked %d, cycles %lld, timeout %lu (now=%llu, abs:%llu)\n", pending, tid,
	       blocked, cycles, thd_timeout, now, tcap_time2cyc(thd_timeout, now));

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
	printc("in the spinner\n");
	while (1)
		;
}

static void
nice(void)
{
	printc("stepped on shit\n");
}

static void
test_timer(void)
{
	int      i;
	thdcap_t tc;
	cycles_t c = 0, p = 0, t = 0;

	PRINTC("Starting timer test.\n");
	nice();
	tc = cos_thd_alloc(&booter_info, booter_info.comp_cap, spinner, NULL);

	for (i = 0; i <= 16; i++) {
		thdid_t     tid;
		int         blocked, rcvd;
		cycles_t    cycles, now;
		tcap_time_t timer, thd_timeout;

		rdtscll(now);

		printc("prepare %llx\n", now);

		timer = tcap_cyc2time(now + 1000 * cyc_per_usec);
		cos_switch(tc, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, 0, timer, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE,
		           cos_sched_sync());
		p = c;
		rdtscll(c);
		if (i > 0) t += c - p;

		printc("out of the spinner \n");
		while (cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, RCV_ALL_PENDING, 0, &rcvd, &tid, &blocked,
		                     &cycles, &thd_timeout)
		       != 0)
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

		ret = cos_tcap_transfer(bt[cos_cpuid()].c.rc, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, i * 100000,
		                        TCAP_PRIO_MAX + 2);
		if (ret) assert(0);

		rdtscll(s);
		if (cos_switch(bt[cos_cpuid()].c.tc, bt[cos_cpuid()].c.tcc, TCAP_PRIO_MAX + 2, TCAP_TIME_NIL,
		               BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, cos_sched_sync()))
			assert(0);
		rdtscll(e);
		PRINTC("\t%lld\n", e - s);

		/* FIXME: we should avoid calling this two times in the common case, return "more evts" */
		while (
		  cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, 0, 0, NULL, &tid, &blocked, &cycles, &thd_timeout)
		  != 0)
			;
	}
	PRINTC("Done.\n");
}

static void
test_budgets_multi(void)
{
	int i;

	PRINTC("Starting multi-level budget test.\n");

	exec_cluster_alloc(&mbt[cos_cpuid()].p, spinner_cyc, &(mbt[cos_cpuid()].p.cyc),
	                   BOOT_CAPTBL_SELF_INITRCV_CPU_BASE);
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

		if (cos_tcap_transfer(mbt[cos_cpuid()].p.rc, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, res,
		                      TCAP_PRIO_MAX + 2))
			assert(0);
		if (cos_tcap_transfer(mbt[cos_cpuid()].c.rc, mbt[cos_cpuid()].p.tcc, res / 2, TCAP_PRIO_MAX + 2))
			assert(0);
		if (cos_tcap_transfer(mbt[cos_cpuid()].g.rc, mbt[cos_cpuid()].c.tcc, res / 4, TCAP_PRIO_MAX + 2))
			assert(0);

		mbt[cos_cpuid()].p.cyc = mbt[cos_cpuid()].c.cyc = mbt[cos_cpuid()].g.cyc = 0;
		rdtscll(s);
		if (cos_switch(mbt[cos_cpuid()].g.tc, mbt[cos_cpuid()].g.tcc, TCAP_PRIO_MAX + 2, TCAP_TIME_NIL,
		               BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, cos_sched_sync()))
			assert(0);
		rdtscll(e);

		cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, 0, 0, NULL, &tid, &blocked, &cycles, &thd_timeout);
		PRINTC("g:%llu c:%llu p:%llu => %llu, %d=%llu\n", mbt[cos_cpuid()].g.cyc - s,
		       mbt[cos_cpuid()].c.cyc - s, mbt[cos_cpuid()].p.cyc - s, e - s, tid, cycles);
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

int wakeup_test_start[NUM_CPU]  = {0};
int wakeup_budget_test[NUM_CPU] = {0};
int active_seq[NUM_CPU]         = {0};
int final_seq[NUM_CPU]          = {0};

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
	at->i.prio              = iprio;
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
	if (cos_tcap_transfer(wat[cos_cpuid()].s.rc, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF,
	                      wat[cos_cpuid()].s.prio))
		assert(0);
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
	at->i.prio              = iprio;
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

long long midinv_cycles[NUM_CPU] = {0LL};

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
	/*
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
	*/
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
	int       i;

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
#define NPAGES 16384
#define STRIDE 384
#define RDTSC_COST 38

unsigned long      data_ptr;
unsigned long long testdata[NPAGES * STRIDE]; /* 100 */

#include <math.h>

static inline void
log_reset(void)
{
	unsigned long long s;
	data_ptr = 0;
	do {
		rdtscll(s);
	} while ((s & 0xFFFFFFFF) > 0x10000000);
	printc("0x%llx - ", s);
}

static inline void
log_data(unsigned long long data)
{
	data               = (data > 38) ? (data - 38) : 0;
	testdata[data_ptr] = data;
	data_ptr++;
}
/* turn off buffers, then cache, fool prefetcher, wraparound with prime number.
 * just read. don't write. */
static unsigned long long
avg_calc(void)
{
	unsigned long long total;
	unsigned long      count;

	total = 0;

	for (count = 0; count < data_ptr; count++) { total += testdata[count]; }

	return total / data_ptr;
}

static unsigned long long
max_calc(void)
{
	unsigned long long max = 0;
	unsigned long      count;

	for (count = 0; count < data_ptr; count++) {
		if (max < testdata[count]) max = testdata[count];
	}

	return max;
}

static unsigned long long
min_calc(void)
{
	unsigned long long min = 0x100000000000;
	unsigned long      count;

	for (count = 0; count < data_ptr; count++) {
		if (min > testdata[count]) min = testdata[count];
	}

	return min;
}

static unsigned long long
print_raw(void)
{
	unsigned long count;

	for (count = 0; count < data_ptr; count++) { printc("%d\n", (int)testdata[count]); }

	return 0;
}

static unsigned long long
sqdev_calc(void)
{
	unsigned long long error_total;
	unsigned long long avg;
	unsigned long      count;

	error_total = 0;
	avg         = avg_calc();

	for (count = 0; count < data_ptr; count++) {
		if (testdata[count] > avg)
			error_total += (testdata[count] - avg) * (testdata[count] - avg);
		else
			error_total += (avg - testdata[count]) * (avg - testdata[count]);
	}

	return error_total / data_ptr;
}

static void
print_result(char *str)
{
	// print_raw();
	printc("%s - avg %lld, stdev %lld, max %lld, min %lld\n", str, avg_calc(),
	       (long long)sqrt((double)sqdev_calc()), max_calc(), min_calc());
}

#define PLD_ADDR(addr) __asm__ __volatile__("pld [%[_addr]] \n\t" ::[_addr] "r"(addr) : "memory", "cc")
#define READ_ADDR(dest, addr)                                                 \
	__asm__ __volatile__("ldr %[_dest], [%[_addr]] \n\t" /* "dsb \n\t" */ \
	                     : [ _dest ] "=r"(dest)                           \
	                     : [ _addr ] "r"(addr)                            \
	                     : "memory", "cc")

#define CAV7_SFR(base, offset) (*((volatile unsigned long *)((unsigned long)((base) + (offset)))))
#define CAV7_L2C_BASE 0xF8F02000
#define CAV7_L2C_D_LOCKDOWN(X) CAV7_SFR(CAV7_L2C_BASE, 0x0900 + (X)*8) /* 0-7 */
#define CAV7_L2C_I_LOCKDOWN(X) CAV7_SFR(CAV7_L2C_BASE, 0x0904 + (X)*8) /* 0-7 */
#define CAV7_L2C_CLEAN_INV_WAY CAV7_SFR(CAV7_L2C_BASE, 0x07FC)
#define CAV7_L2C_CACHE_SYNC CAV7_SFR(CAV7_L2C_BASE, 0x0730)
#define CAV7_L2C_DEBUG_CTRL CAV7_SFR(CAV7_L2C_BASE, 0x0F40)

void
clean_l2(void)
{
	/* Disable line fills first */
	CAV7_L2C_DEBUG_CTRL = 0x3U;
	/* Invalidate all of them and wait */
	CAV7_L2C_CLEAN_INV_WAY = 0xFFFF;
	while ((CAV7_L2C_CLEAN_INV_WAY & 0xFFFF) != 0)
		;
	while (CAV7_L2C_CACHE_SYNC != 0)
		;
	CAV7_L2C_DEBUG_CTRL = 0x00;
}

//#define L2W8_4WAY

void
clean_l2_way8(void)
{
#ifndef L2W8_4WAY
	/* Disable line fills first */
	CAV7_L2C_DEBUG_CTRL = 0x3U;
	/* Invalidate way 8 and wait */
	CAV7_L2C_CLEAN_INV_WAY = 0x80;
	while ((CAV7_L2C_CLEAN_INV_WAY & 0x80) != 0)
		;
	while (CAV7_L2C_CACHE_SYNC != 0)
		;
	CAV7_L2C_DEBUG_CTRL = 0x00;
#else
	/* Disable line fills first */
	CAV7_L2C_DEBUG_CTRL = 0x3U;
	/* Invalidate way 4-8 and wait */
	CAV7_L2C_CLEAN_INV_WAY = 0xF0;
	while ((CAV7_L2C_CLEAN_INV_WAY & 0xF0) != 0)
		;
	while (CAV7_L2C_CACHE_SYNC != 0)
		;
	CAV7_L2C_DEBUG_CTRL = 0x00;
#endif
}

void
clean_all_cache(int param)
{
	cos_hw_l1flush(BOOT_CAPTBL_SELF_INITHW_BASE);

	if (param == 0)
		clean_l2_way8();
	else
		clean_l2();
}

/* Addr 1 - len 1 will be locked down into way 1, Addr 2 - len 2 will be locked down into way 2, etc */
void
lockdown_into_l2(unsigned long addr_1, unsigned long len_1, unsigned long addr_2, unsigned long len_2,
                 unsigned long addr_3, unsigned long len_3, unsigned long addr_4, unsigned long len_4,
                 unsigned long addr_5, unsigned long len_5, unsigned long addr_6, unsigned long len_6,
                 unsigned long addr_7, unsigned long len_7)
{
	unsigned long count;
	unsigned long dest;

#ifndef L2W8_4WAY
	/* Instruction only allowed in the last way */
	CAV7_L2C_I_LOCKDOWN(0) = 0xFF7F;
	__asm__ __volatile__("dsb \n\t"
	                     "isb \n\t" ::
	                       : "memory", "cc");

	/* Perform first lockdown */
	CAV7_L2C_D_LOCKDOWN(0) = 0xFFFE;
	__asm__ __volatile__("dsb \n\t"
	                     "isb \n\t" ::
	                       : "memory", "cc");
	for (count = 0; count < len_1; count += 4) READ_ADDR(dest, addr_1 + count);
	__asm__ __volatile__("dsb \n\t"
	                     "isb \n\t" ::
	                       : "memory", "cc");
	CAV7_L2C_D_LOCKDOWN(0) = 0x0001;
	__asm__ __volatile__("dsb \n\t"
	                     "isb \n\t" ::
	                       : "memory", "cc");

	/* Second */
	CAV7_L2C_D_LOCKDOWN(0) = 0xFFFD;
	__asm__ __volatile__("dsb \n\t"
	                     "isb \n\t" ::
	                       : "memory", "cc");
	for (count = 0; count < len_2; count += 4) READ_ADDR(dest, addr_2 + count);
	__asm__ __volatile__("dsb \n\t"
	                     "isb \n\t" ::
	                       : "memory", "cc");
	CAV7_L2C_D_LOCKDOWN(0) = 0x0003;
	__asm__ __volatile__("dsb \n\t"
	                     "isb \n\t" ::
	                       : "memory", "cc");

	/* Third */
	CAV7_L2C_D_LOCKDOWN(0) = 0xFFFB;
	__asm__ __volatile__("dsb \n\t"
	                     "isb \n\t" ::
	                       : "memory", "cc");
	for (count = 0; count < len_3; count += 4) READ_ADDR(dest, addr_3 + count);
	__asm__ __volatile__("dsb \n\t"
	                     "isb \n\t" ::
	                       : "memory", "cc");
	CAV7_L2C_D_LOCKDOWN(0) = 0x0007;
	__asm__ __volatile__("dsb \n\t"
	                     "isb \n\t" ::
	                       : "memory", "cc");

	/* Fourth */
	CAV7_L2C_D_LOCKDOWN(0) = 0xFFF7;
	__asm__ __volatile__("dsb \n\t"
	                     "isb \n\t" ::
	                       : "memory", "cc");
	for (count = 0; count < len_4; count += 4) READ_ADDR(dest, addr_4 + count);
	__asm__ __volatile__("dsb \n\t"
	                     "isb \n\t" ::
	                       : "memory", "cc");
	CAV7_L2C_D_LOCKDOWN(0) = 0x000F;
	__asm__ __volatile__("dsb \n\t"
	                     "isb \n\t" ::
	                       : "memory", "cc");

	/* Fifth */
	CAV7_L2C_D_LOCKDOWN(0) = 0xFFEF;
	__asm__ __volatile__("dsb \n\t"
	                     "isb \n\t" ::
	                       : "memory", "cc");
	for (count = 0; count < len_5; count += 4) READ_ADDR(dest, addr_5 + count);
	__asm__ __volatile__("dsb \n\t"
	                     "isb \n\t" ::
	                       : "memory", "cc");
	CAV7_L2C_D_LOCKDOWN(0) = 0x001F;
	__asm__ __volatile__("dsb \n\t"
	                     "isb \n\t" ::
	                       : "memory", "cc");

	/* Sixth */
	CAV7_L2C_D_LOCKDOWN(0) = 0xFFDF;
	__asm__ __volatile__("dsb \n\t"
	                     "isb \n\t" ::
	                       : "memory", "cc");
	for (count = 0; count < len_6; count += 4) READ_ADDR(dest, addr_6 + count);
	__asm__ __volatile__("dsb \n\t"
	                     "isb \n\t" ::
	                       : "memory", "cc");
	CAV7_L2C_D_LOCKDOWN(0) = 0x003F;
	__asm__ __volatile__("dsb \n\t"
	                     "isb \n\t" ::
	                       : "memory", "cc");

	/* Seventh */
	CAV7_L2C_D_LOCKDOWN(0) = 0xFFBF;
	__asm__ __volatile__("dsb \n\t"
	                     "isb \n\t" ::
	                       : "memory", "cc");
	for (count = 0; count < len_7; count += 4) READ_ADDR(dest, addr_7 + count);
	__asm__ __volatile__("dsb \n\t"
	                     "isb \n\t" ::
	                       : "memory", "cc");
	CAV7_L2C_D_LOCKDOWN(0) = 0x007F;
	__asm__ __volatile__("dsb \n\t"
	                     "isb \n\t" ::
	                       : "memory", "cc");
#else
	lockdown_into_l2_4way(addr_1, len_1, addr_2, len_2, addr_3, len_3, addr_4, len_4);
#endif
}

void
lockdown_into_l2_4way(unsigned long addr_1, unsigned long len_1, unsigned long addr_2, unsigned long len_2,
                      unsigned long addr_3, unsigned long len_3, unsigned long addr_4, unsigned long len_4)
{
	unsigned long count;
	unsigned long dest;

	/* Instruction only allowed in the last 4 ways */
	CAV7_L2C_I_LOCKDOWN(0) = 0xFF0F;
	__asm__ __volatile__("dsb \n\t"
	                     "isb \n\t" ::
	                       : "memory", "cc");

	/* Perform first lockdown */
	CAV7_L2C_D_LOCKDOWN(0) = 0xFFFE;
	__asm__ __volatile__("dsb \n\t"
	                     "isb \n\t" ::
	                       : "memory", "cc");
	for (count = 0; count < len_1; count += 4) READ_ADDR(dest, addr_1 + count);
	__asm__ __volatile__("dsb \n\t"
	                     "isb \n\t" ::
	                       : "memory", "cc");
	CAV7_L2C_D_LOCKDOWN(0) = 0x0001;
	__asm__ __volatile__("dsb \n\t"
	                     "isb \n\t" ::
	                       : "memory", "cc");

	/* Second */
	CAV7_L2C_D_LOCKDOWN(0) = 0xFFFD;
	__asm__ __volatile__("dsb \n\t"
	                     "isb \n\t" ::
	                       : "memory", "cc");
	for (count = 0; count < len_2; count += 4) READ_ADDR(dest, addr_2 + count);
	__asm__ __volatile__("dsb \n\t"
	                     "isb \n\t" ::
	                       : "memory", "cc");
	CAV7_L2C_D_LOCKDOWN(0) = 0x0003;
	__asm__ __volatile__("dsb \n\t"
	                     "isb \n\t" ::
	                       : "memory", "cc");

	/* Third */
	CAV7_L2C_D_LOCKDOWN(0) = 0xFFFB;
	__asm__ __volatile__("dsb \n\t"
	                     "isb \n\t" ::
	                       : "memory", "cc");
	for (count = 0; count < len_3; count += 4) READ_ADDR(dest, addr_3 + count);
	__asm__ __volatile__("dsb \n\t"
	                     "isb \n\t" ::
	                       : "memory", "cc");
	CAV7_L2C_D_LOCKDOWN(0) = 0x0007;
	__asm__ __volatile__("dsb \n\t"
	                     "isb \n\t" ::
	                       : "memory", "cc");

	/* Fourth */
	CAV7_L2C_D_LOCKDOWN(0) = 0xFFF7;
	__asm__ __volatile__("dsb \n\t"
	                     "isb \n\t" ::
	                       : "memory", "cc");
	for (count = 0; count < len_4; count += 4) READ_ADDR(dest, addr_4 + count);
	__asm__ __volatile__("dsb \n\t"
	                     "isb \n\t" ::
	                       : "memory", "cc");
	CAV7_L2C_D_LOCKDOWN(0) = 0x000F;
	__asm__ __volatile__("dsb \n\t"
	                     "isb \n\t" ::
	                       : "memory", "cc");
}

void
l2_lock_num_test(int ways)
{
	switch (ways) {
	case 0:
		break;
	case 1:
		CAV7_L2C_I_LOCKDOWN(0) = 0xFF01;
		CAV7_L2C_D_LOCKDOWN(0) = 0xFF01;
		break;
	case 2:
		CAV7_L2C_I_LOCKDOWN(0) = 0xFF03;
		CAV7_L2C_D_LOCKDOWN(0) = 0xFF03;
		break;
	case 3:
		CAV7_L2C_I_LOCKDOWN(0) = 0xFF07;
		CAV7_L2C_D_LOCKDOWN(0) = 0xFF07;
		break;
	case 4:
		CAV7_L2C_I_LOCKDOWN(0) = 0xFF0F;
		CAV7_L2C_D_LOCKDOWN(0) = 0xFF0F;
		break;
	case 5:
		CAV7_L2C_I_LOCKDOWN(0) = 0xFF1F;
		CAV7_L2C_D_LOCKDOWN(0) = 0xFF1F;
		break;
	case 6:
		CAV7_L2C_I_LOCKDOWN(0) = 0xFF3F;
		CAV7_L2C_D_LOCKDOWN(0) = 0xFF3F;
		break;
	case 7:
		CAV7_L2C_I_LOCKDOWN(0) = 0xFF7F;
		CAV7_L2C_D_LOCKDOWN(0) = 0xFF7F;
		break;
	case 8:
		CAV7_L2C_I_LOCKDOWN(0) = 0xFFFF;
		CAV7_L2C_D_LOCKDOWN(0) = 0xFFFF;
		break;
	}
}

/* This is for waiting the finish of SDRAM command scheduling */
static void
delay_sched(void)
{
	unsigned long long s;
	unsigned long long e;

	rdtscll(s);

	do {
		rdtscll(e);
	} while ((e - s) < (unsigned long long)0x10000);
}

void
empty_test(void)
{
	unsigned long long s;
	unsigned long long e;
	unsigned long      count;
	log_reset();

	for (count = 0; count < NPAGES * STRIDE; count++) {
		rdtscll(s);
		rdtscll(e);
		log_data(e - s);
	}

	print_result("empty_test");
}

/* Run with TLB lockdown mode, need to give address */
#define TLB_LOCKDOWN 1
#define L2_LOCKDOWN 2
#define SRAM_TEST 3
#define ALL_FLUSH 4
#define TLB_FLUSH 5
#define L2_FLUSH 6
#define L1_FLUSH 7
#define ALIASING 8
#define DO_NOTHING 9

#define NUM_ACCESSES 256 * 1024


void
print_type(unsigned int type)
{
	switch (type) {
	case TLB_LOCKDOWN:
		printc("TLB_LOCKDOWN ");
		break;
	case L2_LOCKDOWN:
		printc("L2_LOCKDOWN ");
		break;
	case SRAM_TEST:
		printc("SRAM_TEST ");
		break;
	case ALL_FLUSH:
		printc("ALL_FLUSH ");
		break;
	case TLB_FLUSH:
		printc("TLB_FLUSH ");
		break;
	case L2_FLUSH:
		printc("L2_FLUSH ");
		break;
	case L1_FLUSH:
		printc("L1_FLUSH ");
		break;
	case ALIASING:
		printc("ALIASING ");
		break;
	case DO_NOTHING:
		printc("DO_NOTHING ");
		break;
	}
}


void
prepare_measurement(unsigned int type, unsigned long vaddr, unsigned long paddr, unsigned long avaddr)
{
	unsigned long dest;

	switch (type) {
	case TLB_LOCKDOWN: {
		cos_hw_tlb_lockdown(BOOT_CAPTBL_SELF_INITHW_BASE, 0, vaddr, paddr);
		clean_all_cache(1);
		break;
	}
	case L2_LOCKDOWN: {
		cos_hw_tlbflush(BOOT_CAPTBL_SELF_INITHW_BASE);
		clean_all_cache(0);
		break;
	}
	case SRAM_TEST: {
		cos_hw_tlbflush(BOOT_CAPTBL_SELF_INITHW_BASE);
		clean_all_cache(1);
		break;
	}
	case ALL_FLUSH: {
		cos_hw_tlbflush(BOOT_CAPTBL_SELF_INITHW_BASE);
		clean_all_cache(1);
		break;
	}
	case TLB_FLUSH: {
		cos_hw_tlbflush(BOOT_CAPTBL_SELF_INITHW_BASE);
		break;
	}
	case L2_FLUSH: {
		clean_all_cache(1);
		break;
	}
	case L1_FLUSH: {
		cos_hw_l1flush(BOOT_CAPTBL_SELF_INITHW_BASE);
		break;
	}
	case ALIASING: {
		cos_hw_tlbflush(BOOT_CAPTBL_SELF_INITHW_BASE);
		clean_all_cache(1);
		READ_ADDR(dest, avaddr);
		break;
	}
	case DO_NOTHING:
		break;
	}
}

/* How long will a TLB miss take, in a read operation? */
void
tlb_test(unsigned long type, unsigned long start, unsigned long accesses)
{
	unsigned char *    ptr = (unsigned char *)start;
	unsigned long long s;
	unsigned long long e;
	unsigned long      count;
	unsigned long      read_addr;
	unsigned long      dest;

	log_reset();
	/* starts from 18347000 */
	for (count = 0; count < accesses; count++) {
		read_addr = (unsigned long)(&ptr[0]);
		/* Prepare the measurement according to the setting */
		prepare_measurement(type, read_addr, 0x20000000 /*0x18347000*/, 0x20000000);
		rdtscll(s);
		READ_ADDR(dest, read_addr);
		rdtscll(e);
		log_data(e - s);
	}

	print_type(type);
	print_result("sequential_test");

	cos_hw_tlb_lockdown(BOOT_CAPTBL_SELF_INITHW_BASE, 0, 0, 0);
	cos_hw_tlbflush(BOOT_CAPTBL_SELF_INITHW_BASE);
	clean_all_cache(1);

	/* The TLB costs a significant amount of overhead. This is crazy (L1, L2 miss).
	 * AVG 163 when all cache flushed but TLB not flushed - likely a TLB hit but L1/L2 miss, TLB lockdown have
	 * similar effects. AVG 243 when page table pinned in cache. AVG 520 when all cache flushed but TLB also
	 * flushed. * AVG 572 59 832 458 alias the page table entries, flush L2. number of locked entries vs. average
	 * performance. */
	/* All cache clean, TLB flushed - avg 159, stdev 40, max 568, min 144
	 *                  nothing done - avg 159, stdev 40, max 520,min 144
	 *                  lockdown     - avg 143, stdev 28, max 410, min 138
	 *                  raw access 298 cycles, no TLB miss included */
}

void
adversarial_workload(void)
{
	cos_hw_tlbflush(BOOT_CAPTBL_SELF_INITHW_BASE);
	clean_all_cache(1);
}

void
adversarial_workload_l2w8(void)
{
	cos_hw_tlbflush(BOOT_CAPTBL_SELF_INITHW_BASE);
	clean_all_cache(0);
}

unsigned int array[1024 * 512 / 4];
void
adversarial_workload_real(void)
{
	int i;
	int temp;
	for (i = 0; i < 1024 * 512 / 4; i++) { READ_ADDR(temp, &array[i]); }
}

void
adversarial_workload_runtime(void)
{
	int                count;
	unsigned int       temp;
	unsigned long long s;
	unsigned long long e;

	log_reset();
	for (count = 0; count < 100000; count++) {
		rdtscll(s);
		adversarial_workload();
		rdtscll(e);
		log_data(e - s);
	}
	print_result("synthetic workload runtime");

	log_reset();
	for (count = 0; count < 1000; count++) {
		rdtscll(s);
		adversarial_workload_real();
		rdtscll(e);
		log_data(e - s);
	}
	print_result("real workload runtime");
}

/* Policy tests - under locked TLB or locked L2 or OCM or nothing, 4 patterns - no aliasing test done on this */
void
sequential_access_test(unsigned long type, unsigned long start, unsigned long accesses, unsigned long interval,
                       capid_t thd)
{
	unsigned char *    ptr = (unsigned char *)start;
	int                count;
	int                addr;
	unsigned long      read_addr;
	unsigned int       temp;
	unsigned long long s;
	unsigned long long e;

	log_reset();

	if (type == TLB_LOCKDOWN) cos_hw_tlb_lockdown(BOOT_CAPTBL_SELF_INITHW_BASE, 12, 0x19000000, 0x19000000);
	;

	for (count = 0; count < accesses; count++) {
		read_addr = (unsigned long)(&ptr[count * 4]);

		if (thd != 0) {
			if ((count % interval) == (interval - 1)) cos_thd_switch(thd);
		}

		//		if (type == TLB_LOCKDOWN)
		//			cos_hw_tlb_lockdown(BOOT_CAPTBL_SELF_INITHW_BASE,0,read_addr,0x18347000 + ((count *
		//4) & 0xFFFFF000));

		rdtscll(s);
		READ_ADDR(temp, read_addr);
		rdtscll(e);
		log_data(e - s);
	}

	print_type(type);
	print_result("sequential_access_test");
	cos_hw_tlb_lockdown(BOOT_CAPTBL_SELF_INITHW_BASE, 0, 0, 0);
	cos_hw_tlbflush(BOOT_CAPTBL_SELF_INITHW_BASE);
}

/* Stride access test - we use some stride access test patterns - repetitively 512 pages, each page access 16 times */
void
stride_access_test(unsigned long type, unsigned long start, unsigned long accesses, unsigned long interval, capid_t thd)
{
	unsigned char *    ptr = (unsigned char *)start;
	int                count;
	int                addr;
	unsigned long      read_addr;
	unsigned int       temp;
	unsigned long long s;
	unsigned long long e;

	log_reset();

	if (type == TLB_LOCKDOWN) cos_hw_tlb_lockdown(BOOT_CAPTBL_SELF_INITHW_BASE, 12, 0x19000000, 0x19000000);
	;

	for (count = 0; count < accesses; count++) {
		addr      = count % (512 * 16);
		addr      = ((addr / 16) * 1024) + (addr % 16);
		read_addr = (unsigned long)(&ptr[addr * 4]);

		if (thd != 0) {
			if ((count % interval) == (interval - 1)) cos_thd_switch(thd);
		}

		//		if (type == TLB_LOCKDOWN)
		//			cos_hw_tlb_lockdown(BOOT_CAPTBL_SELF_INITHW_BASE,0,read_addr,0x18347000 + ((addr *
		//4) & 0xFFFFF000));

		rdtscll(s);
		READ_ADDR(temp, read_addr);
		rdtscll(e);
		log_data(e - s);
	}

	print_type(type);
	print_result("stride_access_test");
	cos_hw_tlb_lockdown(BOOT_CAPTBL_SELF_INITHW_BASE, 0, 0, 0);
	cos_hw_tlbflush(BOOT_CAPTBL_SELF_INITHW_BASE);
}

unsigned short lfsr_reg_user = 0xACE1;
unsigned int
lfsr_user(void)
{
	unsigned short lsb = lfsr_reg_user & 1; /* Get LSB (i.e., the output bit). */
	lfsr_reg_user >>= 1;                    /* Shift register */
	if (lsb) {                              /* If the output bit is 1, apply toggle mask. */
		lfsr_reg_user ^= 0xB400u;
	}

	return lfsr_reg_user;
}

void //__attribute__((optimize("O1")))
random_access_test(unsigned long type, unsigned long start, unsigned long accesses, unsigned long interval, capid_t thd)
{
	unsigned char *    ptr = (unsigned char *)start;
	int                count;
	int                addr;
	unsigned long      read_addr;
	unsigned int       temp;
	unsigned long long s;
	unsigned long long e;

	log_reset();

	if (type == TLB_LOCKDOWN) cos_hw_tlb_lockdown(BOOT_CAPTBL_SELF_INITHW_BASE, 12, 0x19000000, 0x19000000);
	;

	for (count = 0; count < accesses; count++) {
		addr      = lfsr_user() % (512 * 1024);
		read_addr = (unsigned long)(&ptr[addr * 4]);

		if (thd != 0) {
			if ((count % interval) == (interval - 1)) cos_thd_switch(thd);
		}

		//		if (type == TLB_LOCKDOWN)
		//			cos_hw_tlb_lockdown(BOOT_CAPTBL_SELF_INITHW_BASE,0,read_addr,0x18347000 + ((addr *
		//4) & 0xFFFFF000));

		rdtscll(s);
		READ_ADDR(temp, read_addr);
		rdtscll(e);
		log_data(e - s);
	}

	print_type(type);
	print_result("random_access_test");
	cos_hw_tlb_lockdown(BOOT_CAPTBL_SELF_INITHW_BASE, 0, 0, 0);
	cos_hw_tlbflush(BOOT_CAPTBL_SELF_INITHW_BASE);
}

#define MAT_SIZE 128

void
matrix_access_test(unsigned long type, unsigned long start, unsigned long accesses, unsigned long interval, capid_t thd)
{
	unsigned char *    ptr = (unsigned char *)start;
	int                count[3];
	int                int_cnt;
	int                addr;
	unsigned long      read_addr;
	unsigned int       temp;
	unsigned long long s;
	unsigned long long e;

	/* For matrix multiplication, we need to do the computation only once, and we will know how many accesses are
	 * there */
	log_reset();

	if (type == TLB_LOCKDOWN) cos_hw_tlb_lockdown(BOOT_CAPTBL_SELF_INITHW_BASE, 12, 0x19000000, 0x19000000);
	;

	int_cnt = 0;
	for (count[0] = 0; count[0] < MAT_SIZE; count[0]++) {
		printc("%d ", count[0]);
		if (count[0] >= 16) break;
		for (count[1] = 0; count[1] < MAT_SIZE; count[1]++) {
			for (count[2] = 0; count[2] < MAT_SIZE; count[2]++) {
				addr      = count[2] * MAT_SIZE + count[0];
				read_addr = (unsigned long)(&ptr[addr * 4]);

				if (thd != 0) {
					if ((int_cnt % interval) == (interval - 1)) cos_thd_switch(thd);
				}

				//				if(type == TLB_LOCKDOWN)
				//					cos_hw_tlb_lockdown(BOOT_CAPTBL_SELF_INITHW_BASE,1,read_addr,0x18347000
				//+ ((addr * 4) & 0xFFFFF000));

				rdtscll(s);
				READ_ADDR(temp, read_addr);
				rdtscll(e);
				log_data(e - s);

				int_cnt++;

				addr      = count[1] * MAT_SIZE + count[2];
				read_addr = (unsigned long)(&ptr[addr * 4]);

				if (thd != 0) {
					if ((int_cnt % interval) == (interval - 1)) cos_thd_switch(thd);
				}

				//				if (type == TLB_LOCKDOWN)
				//					cos_hw_tlb_lockdown(BOOT_CAPTBL_SELF_INITHW_BASE,0,read_addr,0x18347000
				//+ ((addr * 4) & 0xFFFFF000));

				rdtscll(s);
				READ_ADDR(temp, read_addr);
				rdtscll(e);
				log_data(e - s);

				int_cnt++;
			}
		}
	}

	print_type(type);
	print_result("matrix_access_test");
	cos_hw_tlb_lockdown(BOOT_CAPTBL_SELF_INITHW_BASE, 0, 0, 0);
	cos_hw_tlbflush(BOOT_CAPTBL_SELF_INITHW_BASE);
}

volatile int thd2_state = 1;

#define TLB_TEST 0
#define ADV_TEST 1
#define L2_TEST 2
#define REAL_TEST 3
#define TLB_ONLY 4
#define IDLE_WORK 5
#define CUSTOM_TEST 6
#define IDLE_LOOP 7
void
tlb_sram_test_wrapper(void)
{
	switch (thd2_state) {
	case TLB_TEST: {
		tlb_test(SRAM_TEST, 0x1A010000, NUM_ACCESSES);
		cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
		break;
	}
	case ADV_TEST: {
		printc("entered case 1");
		while (1) {
			adversarial_workload();
			cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
		}
		break;
	}
	case L2_TEST: {
		printc("entered case 2");
		while (1) {
			adversarial_workload_l2w8();
			cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
		}
		break;
	}
	case REAL_TEST: {
		printc("entered case 3");
		while (1) {
			adversarial_workload_real();
			cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
		}
		break;
	}
	case TLB_ONLY: {
		printc("entered case 4");
		while (1) {
			cos_hw_tlbflush(BOOT_CAPTBL_SELF_INITHW_BASE);
			cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
		}
		break;
	}
	case IDLE_WORK: {
		printc("entered case 5");
		while (1) { cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE); }
		break;
	}
	case CUSTOM_TEST: {
		printc("entered case 6");
		sequential_access_test(DO_NOTHING, 0x1A010000, NUM_ACCESSES, 1 /*NUM_ACCESSES*/, 0);
		stride_access_test(DO_NOTHING, 0x1A010000, NUM_ACCESSES, 1 /*NUM_ACCESSES*/, 0);
		random_access_test(DO_NOTHING, 0x1A010000, NUM_ACCESSES, 1 /*NUM_ACCESSES*/, 0);
		matrix_access_test(DO_NOTHING, 0x1A010000, NUM_ACCESSES, 1 /*NUM_ACCESSES*/, 0);
		printc("done\n");
		//			sequential_access_test(SRAM_TEST, 0x1A010000, NUM_ACCESSES, 1/*NUM_ACCESSES*/,
		//BOOT_CAPTBL_SELF_INITTHD_BASE); 			sequential_access_test(SRAM_TEST, 0x1A010000, NUM_ACCESSES,
		//2/*NUM_ACCESSES*/, BOOT_CAPTBL_SELF_INITTHD_BASE); 			sequential_access_test(SRAM_TEST, 0x1A010000,
		//NUM_ACCESSES, 3/*NUM_ACCESSES*/, BOOT_CAPTBL_SELF_INITTHD_BASE); 			sequential_access_test(SRAM_TEST,
		//0x1A010000, NUM_ACCESSES, 4/*NUM_ACCESSES*/, BOOT_CAPTBL_SELF_INITTHD_BASE);
		//			sequential_access_test(SRAM_TEST, 0x1A010000, NUM_ACCESSES, 5/*NUM_ACCESSES*/,
		//BOOT_CAPTBL_SELF_INITTHD_BASE); 			sequential_access_test(SRAM_TEST, 0x1A010000, NUM_ACCESSES,
		//6/*NUM_ACCESSES*/, BOOT_CAPTBL_SELF_INITTHD_BASE); 			sequential_access_test(SRAM_TEST, 0x1A010000,
		//NUM_ACCESSES, 8/*NUM_ACCESSES*/, BOOT_CAPTBL_SELF_INITTHD_BASE); 			sequential_access_test(SRAM_TEST,
		//0x1A010000, NUM_ACCESSES, 16/*NUM_ACCESSES*/, BOOT_CAPTBL_SELF_INITTHD_BASE);
		//			sequential_access_test(SRAM_TEST, 0x1A010000, NUM_ACCESSES, 256/*NUM_ACCESSES*/,
		//BOOT_CAPTBL_SELF_INITTHD_BASE);
		//
		//			stride_access_test(SRAM_TEST, 0x1A010000, NUM_ACCESSES, 1/*NUM_ACCESSES*/,
		//BOOT_CAPTBL_SELF_INITTHD_BASE); 			stride_access_test(SRAM_TEST, 0x1A010000, NUM_ACCESSES,
		//2/*NUM_ACCESSES*/, BOOT_CAPTBL_SELF_INITTHD_BASE); 			stride_access_test(SRAM_TEST, 0x1A010000,
		//NUM_ACCESSES, 3/*NUM_ACCESSES*/, BOOT_CAPTBL_SELF_INITTHD_BASE); 			stride_access_test(SRAM_TEST,
		//0x1A010000, NUM_ACCESSES, 4/*NUM_ACCESSES*/, BOOT_CAPTBL_SELF_INITTHD_BASE);
		//			stride_access_test(SRAM_TEST, 0x1A010000, NUM_ACCESSES, 5/*NUM_ACCESSES*/,
		//BOOT_CAPTBL_SELF_INITTHD_BASE); 			stride_access_test(SRAM_TEST, 0x1A010000, NUM_ACCESSES,
		//6/*NUM_ACCESSES*/, BOOT_CAPTBL_SELF_INITTHD_BASE); 			stride_access_test(SRAM_TEST, 0x1A010000,
		//NUM_ACCESSES, 8/*NUM_ACCESSES*/, BOOT_CAPTBL_SELF_INITTHD_BASE); 			stride_access_test(SRAM_TEST,
		//0x1A010000, NUM_ACCESSES, 16/*NUM_ACCESSES*/, BOOT_CAPTBL_SELF_INITTHD_BASE);
		//			stride_access_test(SRAM_TEST, 0x1A010000, NUM_ACCESSES, 256/*NUM_ACCESSES*/,
		//BOOT_CAPTBL_SELF_INITTHD_BASE);
		//
		//			random_access_test(SRAM_TEST, 0x1A010000, NUM_ACCESSES, 1/*NUM_ACCESSES*/,
		//BOOT_CAPTBL_SELF_INITTHD_BASE); 			random_access_test(SRAM_TEST, 0x1A010000, NUM_ACCESSES,
		//2/*NUM_ACCESSES*/, BOOT_CAPTBL_SELF_INITTHD_BASE); 			random_access_test(SRAM_TEST, 0x1A010000,
		//NUM_ACCESSES, 3/*NUM_ACCESSES*/, BOOT_CAPTBL_SELF_INITTHD_BASE); 			random_access_test(SRAM_TEST,
		//0x1A010000, NUM_ACCESSES, 4/*NUM_ACCESSES*/, BOOT_CAPTBL_SELF_INITTHD_BASE);
		//			random_access_test(SRAM_TEST, 0x1A010000, NUM_ACCESSES, 5/*NUM_ACCESSES*/,
		//BOOT_CAPTBL_SELF_INITTHD_BASE); 			random_access_test(SRAM_TEST, 0x1A010000, NUM_ACCESSES,
		//6/*NUM_ACCESSES*/, BOOT_CAPTBL_SELF_INITTHD_BASE); 			random_access_test(SRAM_TEST, 0x1A010000,
		//NUM_ACCESSES, 8/*NUM_ACCESSES*/, BOOT_CAPTBL_SELF_INITTHD_BASE); 			random_access_test(SRAM_TEST,
		//0x1A010000, NUM_ACCESSES, 16/*NUM_ACCESSES*/, BOOT_CAPTBL_SELF_INITTHD_BASE);
		//			random_access_test(SRAM_TEST, 0x1A010000, NUM_ACCESSES, 256/*NUM_ACCESSES*/,
		//BOOT_CAPTBL_SELF_INITTHD_BASE);
		//
		//			matrix_access_test(SRAM_TEST, 0x1A010000, NUM_ACCESSES, 1/*NUM_ACCESSES*/,
		//BOOT_CAPTBL_SELF_INITTHD_BASE); 			matrix_access_test(SRAM_TEST, 0x1A010000, NUM_ACCESSES,
		//2/*NUM_ACCESSES*/, BOOT_CAPTBL_SELF_INITTHD_BASE); 			matrix_access_test(SRAM_TEST, 0x1A010000,
		//NUM_ACCESSES, 3/*NUM_ACCESSES*/, BOOT_CAPTBL_SELF_INITTHD_BASE); 			matrix_access_test(SRAM_TEST,
		//0x1A010000, NUM_ACCESSES, 4/*NUM_ACCESSES*/, BOOT_CAPTBL_SELF_INITTHD_BASE);
		//			matrix_access_test(SRAM_TEST, 0x1A010000, NUM_ACCESSES, 5/*NUM_ACCESSES*/,
		//BOOT_CAPTBL_SELF_INITTHD_BASE); 			matrix_access_test(SRAM_TEST, 0x1A010000, NUM_ACCESSES,
		//6/*NUM_ACCESSES*/, BOOT_CAPTBL_SELF_INITTHD_BASE); 			matrix_access_test(SRAM_TEST, 0x1A010000,
		//NUM_ACCESSES, 8/*NUM_ACCESSES*/, BOOT_CAPTBL_SELF_INITTHD_BASE); 			matrix_access_test(SRAM_TEST,
		//0x1A010000, NUM_ACCESSES, 16/*NUM_ACCESSES*/, BOOT_CAPTBL_SELF_INITTHD_BASE);
		//			matrix_access_test(SRAM_TEST, 0x1A010000, NUM_ACCESSES, 256/*NUM_ACCESSES*/,
		//BOOT_CAPTBL_SELF_INITTHD_BASE);
	}
	case IDLE_LOOP:
		printc("thd2 in idle loop!\n");
		break;
	}

	while (1)
		;
}

void
abort(void)
{
	while (1)
		;
}

int
sqlite3_os_init(void)
{
	return SQLITE_OK;
}

int
sqlite3_os_end(void)
{
	return SQLITE_OK;
}

int
_write(void)
{
	while (1)
		;
}

int
_close(void)
{
	while (1)
		;
}

int
_lseek(void)
{
	while (1)
		;
}

int
_read(void)
{
	while (1)
		;
}

/*
 * Arguments:
 *
 *   unused - Ignored in this case, see the documentation for sqlite3_exec
 *    count - The number of columns in the result set
 *     data - The row's data
 *  columns - The column names
 */
static int
my_special_callback(void *unused, int count, char **data, char **columns)
{
	int idx;

	// printc("There are %d column(s)\n\r", count);

	for (idx = 0; idx < count; idx++) {
		// printc("The data in column \"%s\" is: %s\n\r", columns[idx], data[idx]);
	}

	// printc("\n\r");

	return 0;
}

unsigned long long
test(int n, ...)
{
	unsigned long long z;

	va_list ap;
	va_start(ap, n);
	z = va_arg(ap, unsigned long long);
	va_end(ap);

	return z;
}

unsigned long long          result;
volatile unsigned long long measure_result;
unsigned long long          start;
unsigned long long          end;
unsigned long long          miss_counter;

extern long  TLSF_Mem_Init(volatile void *Pool, unsigned long Size);
extern void *TLSF_Malloc(volatile void *Pool, unsigned long Size);
extern void  TLSF_Free(volatile void *Pool, void *Mem_Ptr);
extern void *TLSF_Realloc(volatile void *Pool, void *Mem_Ptr, unsigned long Size);

unsigned long SDRAM = 0;

void *
my_malloc(size_t size)
{
	void *ret;

	if (size == 0) size = 1;

	ret = TLSF_Malloc(SDRAM, size);

	// printc("malloc addr 0x%x size 0x%x\n\r", ret, size);

	if (ret < SDRAM)
		while (1)
			;

	return ret;
}

void *
my_pool_malloc(void *udata, size_t size)
{
	return my_malloc(size);
}

void
my_free(void *ptr)
{
	// printc("free addr 0x%x\n\r", ptr);

	TLSF_Free(SDRAM, ptr);
}

void *
my_pool_free(void *udata, void *ptr)
{
	my_free(ptr);
}

void *
my_realloc(void *ptr, size_t size)
{
	void *ret;

	ret = TLSF_Realloc(SDRAM, ptr, size);

	// printc("realloc old 0x%x new 0x%x size 0x%x\n\r", ptr, ret, size);

	return ret;
}

void *
my_pool_realloc(void *udata, void *ptr, size_t size)
{
	return my_realloc(ptr, size);
}

int
print_str(char *str)
{
	printc("%s", str);
	return 0;
}

int
print_hex(unsigned long hex)
{
	printc("0x%x\n", hex);
	return 0;
}

char sqle[256];

/* This is the test code for sqlite */
test_thd_1_sqlite(void)
{
	const char *data    = "Callback function called";
	sqlite3 *   db      = NULL;
	char *      zErrMsg = 0;
	int         rc;
	char *      sql;
	int         count, round;

	/* 8MB memory pool */
	TLSF_Mem_Init(SDRAM, 0x400000);

	rc = fs_register();
	if (rc != SQLITE_OK) { abort(); }

	// printc("========== register complete ==========\n\r");
	rc = sqlite3_open_v2(":memory:", &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);
	if (rc) { abort(); }

	// printc("========== open complete ==========\n\r");
	sql = "PRAGMA journal_mode=OFF;";                  /* Create SQL statement */
	rc  = sqlite3_exec(db, sql, NULL, NULL, &zErrMsg); /* Execute SQL statement */
	if (rc != SQLITE_OK) {
		sqlite3_free(zErrMsg);
		abort();
	}

	// printc("========== journal complete ==========\n\r");
	sql = "CREATE TABLE Cars(Id INT, Name TEXT, Price INT);"; /* Create SQL statement */
	rc  = sqlite3_exec(db, sql, NULL, NULL, &zErrMsg);        /* Execute SQL statement */
	if (rc != SQLITE_OK) {
		sqlite3_free(zErrMsg);
		abort();
	}

	log_reset();
	for (round = 0; round < 10; round++) {
		rdtscll(start);

		cos_hw_tlbstall_recount(BOOT_CAPTBL_SELF_INITHW_BASE);
		/* We may need some test here - insert 20000 statements */
		for (count = 0; count < 16384; count++) {
			sprintf(sqle, "INSERT INTO Cars VALUES(%d,'%x',%d);", count, count * 13, count * 245);
			sql = sqle;
			// sql = "INSERT INTO Cars VALUES(1, 'Audi', 52642);"/* Create SQL statement */;

			rc = sqlite3_exec(db, sql, NULL, NULL, &zErrMsg); /* Execute SQL statement */
			if (rc != SQLITE_OK) {
				sqlite3_free(zErrMsg);
				abort();
			}
		}

		/* Delete all from it */
		sql = "DELETE FROM Cars WHERE (Id % 3 == 0);";
		rc  = sqlite3_exec(db, sql, NULL, NULL, &zErrMsg); /* Execute SQL statement */
		if (rc != SQLITE_OK) {
			sqlite3_free(zErrMsg);
			abort();
		}

		/* Update some records */
		sql = "UPDATE Cars SET Name = 'OhMyGod', Price = 4242 WHERE (Id % 3 == 1);";
		rc  = sqlite3_exec(db, sql, NULL, NULL, &zErrMsg); /* Execute SQL statement */
		if (rc != SQLITE_OK) {
			sqlite3_free(zErrMsg);
			abort();
		}

		/* Output all records from it */
		sql = "SELECT * from Cars WHERE (Id % 3 == 2);";                          /* Create SQL statement */
		rc  = sqlite3_exec(db, sql, my_special_callback, (void *)data, &zErrMsg); /* Execute SQL statement */
		if (rc != SQLITE_OK) {
			sqlite3_free(zErrMsg);
			abort();
		}

		/* Delete all records and start over again */
		sql = "DELETE FROM Cars;";
		rc  = sqlite3_exec(db, sql, NULL, NULL, &zErrMsg); /* Execute SQL statement */
		if (rc != SQLITE_OK) {
			sqlite3_free(zErrMsg);
			abort();
		}
		rdtscll(end);
		// printc("TLB stall %d\n", cos_hw_tlbstall(BOOT_CAPTBL_SELF_INITHW_BASE));
		// log_data(cos_hw_tlbstall(BOOT_CAPTBL_SELF_INITHW_BASE));
		log_data(end - start);
	}

	sqlite3_close(db);
	print_result("sqlite");
}

/* Duktape test run */
/* The time is always some fixed value */
time_t
time(time_t *time)
{
	return 10;
}
static unsigned char buf[1024];

static duk_ret_t
native_print(duk_context *ctx)
{
	duk_push_string(ctx, " ");
	duk_insert(ctx, 0);
	duk_join(ctx, duk_get_top(ctx) - 1);
	sprintf(buf, "%s\n", duk_to_string(ctx, -1));
	// printc(buf);
	return 0;
}

static duk_ret_t
eval_raw(duk_context *ctx, void *udata)
{
	(void)udata;
	duk_eval(ctx);
	return 1;
}

static duk_ret_t
tostring_raw(duk_context *ctx, void *udata)
{
	(void)udata;
	duk_to_string(ctx, -1);
	return 1;
}

static void
usage_exit(void)
{
	while (1)
		;
}

/* Duktape now works */
void
test_duktape_rawspeed(void)
{
	duk_context *ctx;
	int          i;
	const char * res;
	int          count;
	const char   argc = 3;
	//	const char* argv[3]={"sss\n",
	//			     "var nac = function me(x) { return x <= 2 ? 2 : me(x-2) * me(x-1) }",
	//			     "nac(15)",
	//	};

	const char *argv[3] = {"sss\n", "function mandel() { \
			    var w = 76, h = 28, iter = 1000; \
			    var i, j, k, c; \
			    var x0, y0, xx, yy, xx2, yy2; \
			    var line; \
			    for (i = 0; i < h; i++) {  \
			        y0 = (i / h) * 2.5 - 1.25; \
			        for (j = 0, line = []; j < w; j++) { \
			            x0 = (j / w) * 3.0 - 2.0; \
			            for (k = 0, xx = 0, yy = 0, c = '#'; k < iter; k++) { \
			                xx2 = xx*xx; yy2 = yy*yy; \
			                if (xx2 + yy2 < 4.0) { \
			                    yy = 2*xx*yy + y0; \
			                    xx = xx2 - yy2 + x0; \
			                } else { \
			                    /* xx^2 + yy^2 >= 4.0 */ \
			                    if (k < 3) { c = '.'; } \
			                    else if (k < 5) { c = ','; } \
			                    else if (k < 10) { c = '-'; } \
			                    else { c = '='; } \
			                    break; \
			                } \
			            } \
			            line.push(c); \
			        } \
			        print(line.join('')); \
			    } \
			}",
	                       "mandel();"};

	if (argc < 2) { usage_exit(); }

	TLSF_Mem_Init(SDRAM, 0x400000);

	ctx = duk_create_heap(my_pool_malloc, my_pool_realloc, my_pool_free, 0, 0);

	duk_push_c_function(ctx, native_print, DUK_VARARGS);
	duk_put_global_string(ctx, "print");

	log_reset();
	for (count = 0; count < 10; count++) {
		rdtscll(start);
		cos_hw_tlbstall_recount(BOOT_CAPTBL_SELF_INITHW_BASE);
		for (i = 1; i < argc; i++) {
			sprintf(buf, "=== eval: '%s' ===\n", argv[i]);
			// print_string(buf);
			duk_push_string(ctx, argv[i]);
			duk_safe_call(ctx, eval_raw, NULL, 1 /*nargs*/, 1 /*nrets*/);
			duk_safe_call(ctx, tostring_raw, NULL, 1 /*nargs*/, 1 /*nrets*/);
			res = duk_get_string(ctx, -1);
			sprintf(buf, "%s\n", res ? res : "null");
			// print_string(buf);
			duk_pop(ctx);
		}
		rdtscll(end);
		// printc("TLB stall %d\n", cos_hw_tlbstall(BOOT_CAPTBL_SELF_INITHW_BASE));
		// log_data(cos_hw_tlbstall(BOOT_CAPTBL_SELF_INITHW_BASE));
		log_data(end - start);
	}

	duk_destroy_heap(ctx);
	print_result("duk-mdl");
}

void
test_duktape_garbage(void)
{
	duk_context *ctx;
	int          i;
	const char * res;
	int          count;
	const char   argc = 6;

	//	const char* argv[3]={"sss\n",
	//			"function test() { \
//			    var arr, brr, crr, drr; \
//			    var i, j; \
//			    for (i = 0; i < 1; i++) { \
//				arr = []; \
//			        brr = []; \
//			        crr = []; \
//			        drr = []; \
//				for (j = 0; j < 25000; j++) { \
//				    arr[j] = 'foobarrr'; \
//				    brr[j] = 'ascecasx'; \
//				    crr[j] = '12345678'; \
//				    drr[j] = 'aksjnxsk'; \
//				} \
//			        for (j = 0; j < 25000; j++) { \
//				    arr[j] = brr[j]; \
//				    crr[j] = drr[j]; \
//				    arr[j] = crr[j]; \
//				    brr[j] = drr[j]; \
//				} \
//			    } \
//			}",
	//			"test();"
	//	};

	const char *argv[6] = {"sss\n",
	                       "jStat_sum = function sum(arr) { \
				var sum = 0; \
				var i = arr.length; \
				while (--i >= 0) \
					sum += arr[i]; \
		  		return sum; \
			};",
	                       "jStat_mean = function mean(arr) { \
				return jStat_sum(arr) / arr.length; \
			};",
	                       "jStat_covariance = function covariance(arr1, arr2) { \
				var u = jStat_mean(arr1); \
				var v = jStat_mean(arr2); \
				var arr1Len = arr1.length; \
				var sq_dev = new Array(arr1Len); \
				var i; \
				for (var i = 0; i < arr1Len; i++) \
					sq_dev[i] = (arr1[i] - u) * (arr2[i] - v); \
				return jStat_sum(sq_dev) / (arr1Len - 1); \
			};",
	                       "function test() { \
				var arr, brr; \
				var i; \
				arr = []; \
				brr = []; \
				for (i = 0; i < 75000; i++) { \
					arr[i] = i; \
					brr[i] = i+1; \
				} \
				return jStat_covariance(arr,brr); \
			}",
	                       "test();"};

	if (argc < 2) { usage_exit(); }

	TLSF_Mem_Init(SDRAM, 0x400000);

	ctx = duk_create_heap(my_pool_malloc, my_pool_realloc, my_pool_free, 0, 0);

	duk_push_c_function(ctx, native_print, DUK_VARARGS);
	duk_put_global_string(ctx, "print");

	log_reset();
	for (count = 0; count < 10; count++) {
		rdtscll(start);
		// cos_hw_tlbstall_recount(BOOT_CAPTBL_SELF_INITHW_BASE);
		for (i = 1; i < argc; i++) {
			// sprintf(buf,"=== eval: '%s' ===\n", argv[i]);
			// print_string(buf);
			duk_push_string(ctx, argv[i]);
			duk_safe_call(ctx, eval_raw, NULL, 1 /*nargs*/, 1 /*nrets*/);
			duk_safe_call(ctx, tostring_raw, NULL, 1 /*nargs*/, 1 /*nrets*/);
			res = duk_get_string(ctx, -1);
			// sprintf(buf,"%s\n", res ? res : "null");
			// print_string(buf);
			duk_pop(ctx);
		}
		rdtscll(end);

		// printc("TLB stall %d\n", cos_hw_tlbstall(BOOT_CAPTBL_SELF_INITHW_BASE));
		// log_data(cos_hw_tlbstall(BOOT_CAPTBL_SELF_INITHW_BASE));
		log_data(end - start);
	}

	duk_destroy_heap(ctx);
	print_result("duk-cov");
}

void
test_cifar_rawspeed(unsigned int mem)
{
	int count;

	/* Do test on cifar10 */
	log_reset();
	for (count = 0; count < 512; count++) {
		rdtscll(start);
		cifar10(mem);
		rdtscll(end);
		log_data(end - start);
	}
	print_result("cifar10");
}

void __attribute__((optimize("O0"))) empty_test2(void)
{
	for (volatile int x = 0; x < 100; x++)
		for (volatile int y = 0; y < 10000; y++)
			;
}

void
test_thd_1_interf(void)
{
	int round, count;

	//	log_reset();
	//	for(round=0;round<10;round++) {
	//		/* Recount TLB stall cycles */
	//		cos_hw_tlbstall_recount(BOOT_CAPTBL_SELF_INITHW_BASE);
	//		for(count=0;count<50;count++) {empty_test2();}
	//		log_data(cos_hw_tlbstall(BOOT_CAPTBL_SELF_INITHW_BASE));
	//		printc("TLB stall %d\n", cos_hw_tlbstall(BOOT_CAPTBL_SELF_INITHW_BASE));
	//	}
	//	print_result("armnn");

	log_reset();
	for (round = 0; round < 10; round++) {
		/* Recount TLB stall cycles */
		cos_hw_tlbstall_recount(BOOT_CAPTBL_SELF_INITHW_BASE);
		for (count = 0; count < 50; count++) { cifar10(SDRAM); }
		log_data(cos_hw_tlbstall(BOOT_CAPTBL_SELF_INITHW_BASE));
		printc("TLB stall %d\n", cos_hw_tlbstall(BOOT_CAPTBL_SELF_INITHW_BASE));
	}
	print_result("armnn");
	test_thd_1_sqlite();
	test_duktape_rawspeed();
	test_duktape_garbage();
	while (1)
		;
}

/* NOTHING                ----                      68.01%                      50.61                     64.26
 *  interval              armnn                      sqlite                      duk-mdl                   duk-cov
 *  10         175619141 72078 175763689   270479176 1889531 273368398  94751250 2846972 100742057   72226153 1961102
 * 76355646 20         75897211  453416 76204925   124032045 728744  124908320  38737986 884000  40635730    26631709
 * 614644  28157099 50         28540382  35058 28594812    49788857  715955  50445960   14098481 250396  14658260
 * 9344921  179236  9794300 100        13917930  16223 13941788    25738141  624516  26181627   6811971  115530  7070477
 * 4549936  93622   4791535 200        6803971   13986 6824961     8207406   201820  8362428    3406228  55923   3537793
 * 2334201  46682   2453256 500        2682353   7656  2693790     2468289   66953   2536219    1415354  26476   1468032
 * 1034473  18811   1083262 1000       1384509   7553  1392887     1141884   30546   1171909    754742   12037   774990
 * 611829   11513 639069 TLB LOCKDOWN interval             armnn                       sqlite duk-mdl duk-cov 10
 * 105725218  42672  105821834  86501544  382804 87317726    44934510 2582458 49615669  26814179 658421 27901826 20
 * 47319313   13941  47341826   35488477  203223 35740309    19503978 778062  21008091  10689875 217619 11032330 50
 * 17918144   8758   17931017   12258755  83471  12372417    7112392  281221  7635257   3831760  79725  3963955 100
 * 8764257    9311   8779862    5835928   31382  5887844     3441853  139849  3686587   1835272  44920  1911487 200
 * 4243237    11733  4268056    1897515   19483  1929663     1729698  68063   1837049   911382   19422  944662 500
 * 1626713    6208   1637442    583149    5017   592960      700077   31586   752985    362073   5294   370450 1000
 * 791291     3137   794609     271704    3289   276904      362749   20199   409182    183885   3629   191508 L2
 * LOCKDOWN - 4 ways locked down ** running interval             armnn                       sqlite duk-mdl duk-cov 10
 * 132302355 45194  132386344  143205561 1119696 144670195  57715344 1251497 60251433  34227591 856556 36180483 20
 * 58566411  29530  58622381   64608011  552612  65352311   24121527 450392  24808373  13421532 272540 14107937 50
 * 21725088  8957   21738909   23954557  281802  24231696   8852877  162820  9058815   4858556  84530  5042831 100
 * 10562192  9146   10572381   11455990  244519  11678393   4302815  78438  4397112    2402256  48212  2534984 200
 * 5194806   9547   5209867    3546016   84699   3656411    2133964  46860  2189308    1254836  27888  1327351 500
 * 2100825   4528   2110432    1082132   36288   1138603    875831   19802  912216     598032   9725   623209 1000
 * 1089871   4174   1097107    501862    20163   533161     462716   15109  498422     382784   6482   393612 OCM **
 * running interval             armnn                       sqlite                      duk-mdl                  duk-cov
 *  10         156222014 25370  156250936  204419538 868399  205772007  66984021 1630336 7095916    47922442 1076878
 * 50587656 20         73986259  108450 74135321   87587925  574112  88465788   26743330 639385  28130111   17698740
 * 359610 18607856 50         28181005  48915  28238819   31352377  253926  31645179   9699600  81378   9903349 6262110
 * 62675  6362051 100        13577993  9837   13590259   14690232  216759  14867090   4680330  102685  4881042 3026373
 * 54338 3173223 200        6565577   5404   6573536    4527315   71430   4579001    2342509  53390   2450917    1556498
 * 29762 1637029 500        2679514   5142   2688830    1316121   26952   1348761    996186   28799   1064177    703148
 * 9821 728259 1000       1340865   3922   1347938    594423    12202   604614     547220   18603   600405     424478
 * 7112  439666
 */
/*



*/

#define WORKLOAD_NAME test_thd_1_interf
#define INTERVAL 10

void
scheduler(thdcap_t thd1, thdcap_t thd2)
{
	thd2_state = L2_TEST;
	tcap_time_t timer;

	// cos_hw_tlb_lockdown(BOOT_CAPTBL_SELF_INITHW_BASE,12,0x19000000,0x19000000);
	cos_hw_tlb_lockdown(BOOT_CAPTBL_SELF_INITHW_BASE, 13, 0x14000000, 0x14000000);
	cos_hw_tlb_lockdown(BOOT_CAPTBL_SELF_INITHW_BASE, 14, 0x11000000, 0x11000000);
	cos_hw_tlb_lockdown(BOOT_CAPTBL_SELF_INITHW_BASE, 15, 0x10000000, 0x10000000);

	// SDRAM = 0x1A010000;

	while (1) {
		/* Switch to the benchmark */
		rdtscll(result);
		result += INTERVAL * 767;
		timer = tcap_cyc2time(result);
		cos_switch(thd1, BOOT_CAPTBL_SELF_INITTCAP_BASE, 0, timer, BOOT_CAPTBL_SELF_INITRCV_BASE,
		           cos_sched_sync());

		/* Switch to the interference workload - always works for once */
		cos_thd_switch(thd2);
	}
}

#define ADDR(X) (*(unsigned long *)(X))

void
test_run_mb(void)
{
	compcap_t compcap;
	thdcap_t  thd;
	/* Allocate about 4MB of memory */
	unsigned long mem = cos_page_bump_allocn(&booter_info, NPAGES * PAGE_SIZE);
	assert(mem);
	/* Alias that 16MB of memory to somewhere else */
	unsigned long alias_mem = cos_pgtbl_intern_alloc(&booter_info, BOOT_CAPTBL_SELF_PT, 0x20000000,
	                                                 NPAGES * PAGE_SIZE);
	assert(alias_mem);
	alias_mem = cos_mem_alias_atn(&booter_info, 0x20000000, &booter_info, mem, NPAGES * PAGE_SIZE);
	assert(alias_mem == 0);
	/* Do some aliasing, mem in cache, TLB is not. See if this aliasing is successful */
	ADDR(mem) = 0x12345678;
	printc("The addr %x is %x\n", mem, ADDR(0x20000000));

	/* Lets create some threads in that component so we can go and measure some stuff... */
	thd = cos_thd_alloc(&booter_info, BOOT_CAPTBL_COMP0_COMP, tlb_sram_test_wrapper, 0);
	assert(thd);
	thd2_state = CUSTOM_TEST;
	//	/* Give RDTSC overhead first */
	//	empty_test();
	//	/* What's the overhead if we do nothing */
	//	tlb_test(DO_NOTHING, mem, NUM_ACCESSES);
	//	/* What's the overhead if we just have a L1 miss? */
	//	tlb_test(L1_FLUSH, mem, NUM_ACCESSES);
	//	/* What's the overhead if everything flushed? */
	//	tlb_test(ALL_FLUSH, mem, NUM_ACCESSES);
	//	/* What's the overhead if L2 is flushed? */
	//	tlb_test(L2_FLUSH, mem, NUM_ACCESSES);
	//	/* What's the overhead if TLB is flushed? */
	//	tlb_test(TLB_FLUSH, mem, NUM_ACCESSES);
	//	/* What's the overhead of aliasing? */
	//	tlb_test(ALIASING, mem, NUM_ACCESSES);
	//	/* What's the overhead if TLB gets locked down? */
	//	tlb_test(TLB_LOCKDOWN, mem, NUM_ACCESSES);
	//	/* What's the overhead if placed in OCSRAM? */
	//	cos_thd_switch(thd);
	//	/* This is as far as we can lock 8*256 = 1024 pages */
	//	/* What's the overhead if L2 gets locked down? */
	//	/* This is locking into way 1 - 7 */
	//	cos_hw_l1flush(BOOT_CAPTBL_SELF_INITHW_BASE);
	//	clean_l2();
	//	lockdown_into_l2(0x98003000,0x1000,
	//			 0x98306000,0x10000,
	//			 0x98316000,0x10000,
	//			 0x98326000,0x10000,
	//			 0x98336000,0x10000,
	//			 0x98336000,0x10000,
	//			 0x98336000,0x10000);
	//	tlb_test(L2_LOCKDOWN, mem, NUM_ACCESSES);
	/*
entered case 2L2_LOCKDOWN sequential_access_test - avg 226, stdev 53, max 524, min 152
0x500000018 - L2_LOCKDOWN stride_access_test - avg 165, stdev 24, max 434, min 152
0x900000022 - L2_LOCKDOWN random_access_test - avg 185, stdev 43, max 484, min 158
0xd0000001c - 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 L2_LOCKDOWN matrix_access_test - avg 166, stdev 35, max 442, min
152
*/

	/* How long will these workload run for? - interesting, synthetic runs slower than real 149608/1609979 */
	// adversarial_workload_runtime();
	/* Run sequential access test - lockdown */
	thd2_state = ADV_TEST;
#define POLICY DO_NOTHING
//#define FUNC_TEST sequential_access_test
//	FUNC_TEST(POLICY, mem, NUM_ACCESSES, 1/*NUM_ACCESSES*/, thd);
//	FUNC_TEST(POLICY, mem, NUM_ACCESSES, 2/*NUM_ACCESSES*/, thd);
//	FUNC_TEST(POLICY, mem, NUM_ACCESSES, 3/*NUM_ACCESSES*/, thd);
//	FUNC_TEST(POLICY, mem, NUM_ACCESSES, 4/*NUM_ACCESSES*/, thd);
//	FUNC_TEST(POLICY, mem, NUM_ACCESSES, 5/*NUM_ACCESSES*/, thd);
//	FUNC_TEST(POLICY, mem, NUM_ACCESSES, 6/*NUM_ACCESSES*/, thd);
//	FUNC_TEST(POLICY, mem, NUM_ACCESSES, 8/*NUM_ACCESSES*/, thd);
//	FUNC_TEST(POLICY, mem, NUM_ACCESSES, 16/*NUM_ACCESSES*/, thd);
//	FUNC_TEST(POLICY, mem, NUM_ACCESSES, 256/*NUM_ACCESSES*/, thd);
//#define FUNC_TEST stride_access_test
//	FUNC_TEST(POLICY, mem, NUM_ACCESSES, 1/*NUM_ACCESSES*/, thd);
//	FUNC_TEST(POLICY, mem, NUM_ACCESSES, 2/*NUM_ACCESSES*/, thd);
//	FUNC_TEST(POLICY, mem, NUM_ACCESSES, 3/*NUM_ACCESSES*/, thd);
//	FUNC_TEST(POLICY, mem, NUM_ACCESSES, 4/*NUM_ACCESSES*/, thd);
//	FUNC_TEST(POLICY, mem, NUM_ACCESSES, 5/*NUM_ACCESSES*/, thd);
//	FUNC_TEST(POLICY, mem, NUM_ACCESSES, 6/*NUM_ACCESSES*/, thd);
//	FUNC_TEST(POLICY, mem, NUM_ACCESSES, 8/*NUM_ACCESSES*/, thd);
//	FUNC_TEST(POLICY, mem, NUM_ACCESSES, 16/*NUM_ACCESSES*/, thd);
//	FUNC_TEST(POLICY, mem, NUM_ACCESSES, 256/*NUM_ACCESSES*/, thd);
#define FUNC_TEST random_access_test
	FUNC_TEST(POLICY, mem, NUM_ACCESSES, 1 /*NUM_ACCESSES*/, thd);
	FUNC_TEST(POLICY, mem, NUM_ACCESSES, 2 /*NUM_ACCESSES*/, thd);
	FUNC_TEST(POLICY, mem, NUM_ACCESSES, 3 /*NUM_ACCESSES*/, thd);
	FUNC_TEST(POLICY, mem, NUM_ACCESSES, 4 /*NUM_ACCESSES*/, thd);
	FUNC_TEST(POLICY, mem, NUM_ACCESSES, 5 /*NUM_ACCESSES*/, thd);
	FUNC_TEST(POLICY, mem, NUM_ACCESSES, 6 /*NUM_ACCESSES*/, thd);
	FUNC_TEST(POLICY, mem, NUM_ACCESSES, 8 /*NUM_ACCESSES*/, thd);
	FUNC_TEST(POLICY, mem, NUM_ACCESSES, 16 /*NUM_ACCESSES*/, thd);
	FUNC_TEST(POLICY, mem, NUM_ACCESSES, 256 /*NUM_ACCESSES*/, thd);
	//#define FUNC_TEST matrix_access_test
	//	FUNC_TEST(POLICY, mem, NUM_ACCESSES, 1/*NUM_ACCESSES*/, thd);
	//	FUNC_TEST(POLICY, mem, NUM_ACCESSES, 2/*NUM_ACCESSES*/, thd);
	//	FUNC_TEST(POLICY, mem, NUM_ACCESSES, 3/*NUM_ACCESSES*/, thd);
	//	FUNC_TEST(POLICY, mem, NUM_ACCESSES, 4/*NUM_ACCESSES*/, thd);
	//	FUNC_TEST(POLICY, mem, NUM_ACCESSES, 5/*NUM_ACCESSES*/, thd);
	//	FUNC_TEST(POLICY, mem, NUM_ACCESSES, 6/*NUM_ACCESSES*/, thd);
	//	FUNC_TEST(POLICY, mem, NUM_ACCESSES, 8/*NUM_ACCESSES*/, thd);
	//	FUNC_TEST(POLICY, mem, NUM_ACCESSES, 16/*NUM_ACCESSES*/, thd);
	//	FUNC_TEST(POLICY, mem, NUM_ACCESSES, 256/*NUM_ACCESSES*/, thd);
	//	thd2_state=CUSTOM_TEST;
	//	cos_thd_switch(thd);
	while (1)
		;

	while (1) {
		adversarial_workload();
		cos_thd_switch(thd);
	}
	//
	//	while(1);
	//	printc("0 ways locked down\n");
	//	cos_hw_l1flush(BOOT_CAPTBL_SELF_INITHW_BASE);
	//	clean_l2();
	//	l2_lock_num_test(0);
	//	sequential_access_test(DO_NOTHING, mem, NUM_ACCESSES, 1/*NUM_ACCESSES*/, 0);
	//	stride_access_test(DO_NOTHING, mem, NUM_ACCESSES, 1/*NUM_ACCESSES*/, 0);
	//	random_access_test(DO_NOTHING, mem, NUM_ACCESSES, 1/*NUM_ACCESSES*/, 0);
	//	matrix_access_test(DO_NOTHING, mem, NUM_ACCESSES, 1/*NUM_ACCESSES*/, 0);
	//
	//	printc("1 ways locked down\n");
	//	cos_hw_l1flush(BOOT_CAPTBL_SELF_INITHW_BASE);
	//	clean_l2();
	//	l2_lock_num_test(1);
	//	sequential_access_test(DO_NOTHING, mem, NUM_ACCESSES, 1/*NUM_ACCESSES*/, 0);
	//	stride_access_test(DO_NOTHING, mem, NUM_ACCESSES, 1/*NUM_ACCESSES*/, 0);
	//	random_access_test(DO_NOTHING, mem, NUM_ACCESSES, 1/*NUM_ACCESSES*/, 0);
	//	matrix_access_test(DO_NOTHING, mem, NUM_ACCESSES, 1/*NUM_ACCESSES*/, 0);
	//
	//	printc("5 ways locked down\n");
	//	cos_hw_l1flush(BOOT_CAPTBL_SELF_INITHW_BASE);
	//	clean_l2();
	//	l2_lock_num_test(5);
	//	sequential_access_test(DO_NOTHING, mem, NUM_ACCESSES, 1/*NUM_ACCESSES*/, 0);
	//	stride_access_test(DO_NOTHING, mem, NUM_ACCESSES, 1/*NUM_ACCESSES*/, 0);
	//	random_access_test(DO_NOTHING, mem, NUM_ACCESSES, 1/*NUM_ACCESSES*/, 0);
	//	matrix_access_test(DO_NOTHING, mem, NUM_ACCESSES, 1/*NUM_ACCESSES*/, 0);
	//
	//	printc("6 ways locked down\n");
	//	cos_hw_l1flush(BOOT_CAPTBL_SELF_INITHW_BASE);
	//	clean_l2();
	//	l2_lock_num_test(6);
	//	sequential_access_test(DO_NOTHING, mem, NUM_ACCESSES, 1/*NUM_ACCESSES*/, 0);
	//	stride_access_test(DO_NOTHING, mem, NUM_ACCESSES, 1/*NUM_ACCESSES*/, 0);
	//	random_access_test(DO_NOTHING, mem, NUM_ACCESSES, 1/*NUM_ACCESSES*/, 0);
	//	matrix_access_test(DO_NOTHING, mem, NUM_ACCESSES, 1/*NUM_ACCESSES*/, 0);
	//
	//	printc("7 ways locked down\n");
	//	cos_hw_l1flush(BOOT_CAPTBL_SELF_INITHW_BASE);
	//	clean_l2();
	//	l2_lock_num_test(7);
	//	sequential_access_test(DO_NOTHING, mem, NUM_ACCESSES, 1/*NUM_ACCESSES*/, 0);
	//	stride_access_test(DO_NOTHING, mem, NUM_ACCESSES, 1/*NUM_ACCESSES*/, 0);
	//	random_access_test(DO_NOTHING, mem, NUM_ACCESSES, 1/*NUM_ACCESSES*/, 0);
	//	matrix_access_test(DO_NOTHING, mem, NUM_ACCESSES, 1/*NUM_ACCESSES*/, 0);
	//
	//	printc("8 ways locked down\n");
	//	cos_hw_l1flush(BOOT_CAPTBL_SELF_INITHW_BASE);
	//	clean_l2();
	//	l2_lock_num_test(8);
	//	sequential_access_test(DO_NOTHING, mem, NUM_ACCESSES, 1/*NUM_ACCESSES*/, 0);
	//	stride_access_test(DO_NOTHING, mem, NUM_ACCESSES, 1/*NUM_ACCESSES*/, 0);
	//	random_access_test(DO_NOTHING, mem, NUM_ACCESSES, 1/*NUM_ACCESSES*/, 0);
	//	matrix_access_test(DO_NOTHING, mem, NUM_ACCESSES, 1/*NUM_ACCESSES*/, 0);

	//	while(1);
	// ALL DATA HERE WRONG - REFER TO LATEST TESTCASE INSTEAD. THESE DATA ARE FOR L1 NOT ENABLED.
	/*               0             1            5               6                 7                8
	 * sequential 143-27-476   143-27-476   143-27-544      143-27-544       145-28-554       595-65-850
	 * stride     99-27-506    99-27-470    99-28-510       106-39-510       127-52-546       345-44-602
	 * random     105-36-442   108-39-448   135-54-472      151-61-512       163-51-536       335-31-604
	 * matrix     92-13-442    92-14-432    94-18-442       95-20-564        100-30-506       342-46-616
	 * armnn      135573753    135644826    135619816       135756487        136380765        579646066
	 *            350          3654         4694            6360             1980             9232
	 *            135581618    135645336    135677004       135784116        136392956        579678464
	 * sqlite     2021387848   2021933514   2097121273      2330601381       2857971900       7248890200
	 *            1740742      2002536      3867183         5665919          5873020          13374974
	 *            2024370570   2024418274   2101814252      2343007294       2867800932       7272448452
	 * duk-mandel 2048777326   2048962142   2049121273      2049897799       2062097481       6543317674
	 *            1546195      1667651      1584258         1576762          34924468         84374181
	 *            2055369536   2056079394   2055918252      2056657058       2214180982       6703936902
	 * duk-cov    1457204954   1457496288   1467529465      1470971259       1480228074       4862995894
	 *            1460416      1534874      1439785         1351337          17618511         64358977
	 *            1459565292   1460017920   1469703482      1473114442       1556929082       4997460314
	 * corrected data
	 * armnn      32536032     32460781     32425956        32468703         32544903         32671140
	 *            4493         56713        58158           57846            57673            56054
	 *            32538652     32560610     32517514        32568850         32647706         32780756
	 * sqlite     663179107    658818777    679014081       787962675        1067650677       1537398710
	 *            797341       901338       1296941         1879988          2616473          2099406
	 *            664993082    661045736    681410014       791789650        1073813548       1542585448
	 * duk-mandel 533424770    533967217    534106200       534199099        536341328        540589126
	 *            181161       275448       293810          301293           177953           325258
	 *            533830650    534601450    534853696       535026472        536638548        541305876
	 * duk-cov    363313901    365226272    373939109       376611105        379654754        377975910
	 *            573446       636066       676625          771838           730431           1068047
	 *            364034718    366130068    375361656       378246366        381726290        379718614
	 *
	 *0ways locked down
0x100000000 - cifar10 - avg 43127218, stdev 3888, max 43131304, min 43046708
0x700000004 - sqlite - avg 654405849, stdev 700774, max 655259080, min 653040610
0x900000022 - duk-mdl - avg 535534559, stdev 152083, max 535746604, min 535238700
0xb00000000 - duk-cov - avg 345374909, stdev 561432, max 346564110, min 344584374
	 *!! id is 3
1 ways locked down
0x10000001e - cifar10 - avg 42929582, stdev 73507, max 43076568, min 42837804
0x70000001e - sqlite - avg 654674818, stdev 662758, max 655365396, min 652972336
0x900000004 - duk-mdl - avg 535129288, stdev 135704, max 535242484, min 534735768
0xb00000020 - duk-cov - avg 349070664, stdev 284830, max 349560000, min 348651748
!! id is 3
5 ways locked down
0x10000000e - cifar10 - avg 42917689, stdev 71352, max 43068974, min 42834090
0x700000024 - sqlite - avg 686458959, stdev 1056509, max 687871794, min 683806702
0x90000001c - duk-mdl - avg 535109336, stdev 81763, max 535254520, min 534976980
0xb00000012 - duk-cov - avg 351898325, stdev 187721, max 352276024, min 351624318
6 ways locked down
0x10000001e - cifar10 - avg 42927839, stdev 77955, max 43066492, min 42831480
0x700000004 - sqlite - avg 785132437, stdev 1508191, max 787064956, min 781472924
0x90000001a - duk-mdl - avg 535361090, stdev 133962, max 535566428, min 535170378
0xb00000020 - duk-cov - avg 352751068, stdev 263471, max 353251294, min 352505092
ern, frame_addr 4182000, frame 1c387000, order 12
!!! id is 3
7 ways locked down
0x100000008 - cifar10 - avg 42915305, stdev 53211, max 43011030, min 42874662
0x700000008 - sqlite - avg 1044673284, stdev 1411186, max 1046379172, min 1041114426
0xa00000012 - duk-mdl - avg 536342437, stdev 105654, max 536521966, min 536199610
0xc00000010 - duk-cov - avg 354362195, stdev 351651, max 354887982, min 353893060
3
8 ways locked down
0x100000022 - cifar10 - avg 43183196, stdev 52862, max 43288440, min 43137594
0x70000000e - sqlite - avg 1470063990, stdev 2354725, max 1473939336, min 1464594284
0xb00000012 - duk-mdl - avg 541133388, stdev 216943, max 541479662, min 540777340
0xd00000014 - duk-cov - avg 388011491, stdev 985990, max 388865546, min 386070552


	 *
	 */

	/* Application benchmarks */
	//	SDRAM = mem;
	////	printc("SDRAM addr is %x\n",SDRAM);
	////	/* See if TCaps really work - yes */
	////
	//	printc("8 ways locked down\n");
	//	cos_hw_l1flush(BOOT_CAPTBL_SELF_INITHW_BASE);
	//	clean_l2();
	//	l2_lock_num_test(8);
	//        /* If we use 8MB here we are perfectly good - same vs that. the TLB lockdown if possible we hardcode
	//        it. */
	//	test_cifar_rawspeed(mem);
	//	test_thd_1_sqlite();
	//	test_duktape_rawspeed();
	//	test_duktape_garbage();

	while (1)
		;
	/*7 ways locked down
	0x100000014 - duk-cov - avg 376885660, stdev 761440, max 379413058, min 375679114 */


	/* Allocate one more thread to run the workload */
	thdcap_t thd2 = cos_thd_alloc(&booter_info, BOOT_CAPTBL_SELF_COMP, WORKLOAD_NAME, 0);
	assert(thd);
	scheduler(thd2, thd);
	while (1)
		;

	while (1) {
		adversarial_workload();
		cos_thd_switch(thd);
	}

	/* sequential      1            2            3            4            5            6           8         16 256
	 * nothing     330-45-592  197-137-592  148-130-592  127-122-594  109-107-566  103-104-594  90-90-592  74-73-594
	 * 57-30-586 TLB lock    126-15-400  99-51-400    81-42-400    78-44-404    72-41-400    71-41-406    67-36-400
	 * 63-33-400  58-28-400 L2 lock     168-15-412  121-66-412   97-63-440    89-59-440    81-57-426    80-57-420
	 * 73-49-442  64-36-462  57-27-442 OCM         161-12-438  114-56-438   92-55-438    85-50-438    78-50-436
	 * 76-49-438    71-43-438  63-34-400  57-29-426
	 *
	 * stride          1            2            3            4            5            6           8         16 256
	 * nothing     328-36-602  199-133-602  149-129-602  130-120-602  113-110-576  107-106-602  97-97-602  78-74-602
	 * 61-39-594 TLB lock    126-15-400  96-39-400    81-42-400    79-45-400    72-43-400    71-42-400    68-38-400
	 * 63-35-400  58-28-400 L2 lock     176-36-442  122-62-442   99-65-442    93-64-484    83-57-442    82-58-442
	 * 76-53-442  68-45-442  59-32-432 OCM         164-25-438  118-58-444   94-59-438    89-57-444    81-53-438
	 * 79-51-438    74-48-444  67-44-448  60-35-438
	 *
	 * random
	 * nothing     514-34-738  382-143-828  298-103-756  301-128-782  270-107-782  260-102-782  252-113-782
	 * 218-93-780 144-51-780 TLB lock    241-42-520  181-58-510   164-62-510   156-56-510   148-51-510   147-51-520
	 * 140-42-520  137-47-510 126-34-510 L2 lock     285-59-544  220-50-544   210-71-544   206-73-544   197-64-544
	 * 188-49-596   186-49-532  175-46-522 138-37-544 OCM         283-32-558  226-67-556   204-62-558   197-57-556
	 * 191-59-558   185-53-556   181-55-556  171-48-556 137-39-554
	 *
	 * matrix
	 * nothing     331-50-594  265-75-594  209-108-594   190-103-592  162-105-594  149-101-594  135-96-646
	 * 110-77-594  92-50-712 TLB lock    203-35-502  171-58-612  142-61-592    129-58-612   122-61-590   116-60-592
	 * 109-55-592  99-50-568   90-45-590 L2 lock     240-25-512  205-48-512  166-67-512    154-72-512   139-77-512
	 * 132-79-512   121-74-512  102-58-512  89-43-512 OCM         199-37-508  195-37-438  153-37-506    145-68-548
	 * 130-67-508   130-75-508   117-67-532  100-54-508  88-42-634
	 */
	while (1)
		;
	// tlb_alias_test(mem, 0x20000000, NPAGES);
	// tlb_test(mem, NPAGES * PAGE_SIZE);
	// local_test(mem);
	/* Different patterns - test 4 lockdown methods, comparatively */
	// sequential_access_test(mem, NPAGES * STRIDE);

	// sequential_tlb_test(mem, NPAGES * PAGE_SIZE);
	// random_tlb_test(mem, NPAGES*PAGE_SIZE);
	// cifar10();
	while (1)
		;
	/* test_ipi(); */
	// test_timer();
	// test_budgets();

	// test_thds();
	// test_thds_perf();

	// test_mem();

	// test_async_endpoints();
	// test_async_endpoints_perf();

	// test_inv();
	// test_inv_perf();

	// test_captbl_expand();

	/* Run some tests against TLB reloading - L2 cache not enabled */


	/*
	 * FIXME: Preemption stack mechanism in the kernel is disabled.
	 * test_wakeup();
	 * test_preemption();
	 */
}

static void
block_vm(void)
{
	int         blocked, rcvd;
	cycles_t    cycles, now;
	tcap_time_t timeout, thd_timeout;
	thdid_t     tid;

	while (cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, RCV_ALL_PENDING | RCV_NON_BLOCKING, 0, &rcvd, &tid,
	                     &blocked, &cycles, &thd_timeout)
	       > 0)
		;

	rdtscll(now);
	now += (1000 * cyc_per_usec);
	timeout = tcap_cyc2time(now);
	cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, RCV_ALL_PENDING, timeout, &rcvd, &tid, &blocked, &cycles,
	              &thd_timeout);
}

void
dbgvar(unsigned long var)
{
	printc("Debug var - %x\n", var);
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

#include <cos_kernel_api.h>
#include <cos_types.h>
#include <ck_ring.h>
#include <inplace_ring.h>
#include <sl.h>
#include <capmgr.h>
#include <cos_rdtsc.h>

/* set the ipi rate for the test */
#define IPI_RATE   10
#define IPI_WIN_US 1000

/* set the send rate. should be same across all tests */
#define SNDRATE 50
#define SNDWINUS 1000

/* set a polling rate for a given test */
#define RCV_POLL_US 10000

#define HI_PRIO 1
#define SND_CORE 0
#define RCV_CORE 1

static volatile asndcap_t sndc = 0;
static volatile arcvcap_t rcvc = 0;

static volatile vaddr_t ipcaddr = NULL;
static volatile struct ck_ring * volatile ipcring = NULL;

#define IPC_BUF_SZ ((64*PAGE_SIZE) + sizeof(struct ck_ring))
static char ipcbuf[IPC_BUF_SZ];

#define TEST_ITERS (1000000)

static volatile unsigned long test_iters CACHE_ALIGNED = 0;
static cycles_t test_cycs[TEST_ITERS] CACHE_ALIGNED = { 0 };

static volatile int rcv_ready = 0;
static volatile int snd_ready = 0;
static volatile cycles_t ck_enq_min = 0, ck_deq_min = 0;

INPLACE_RING_BUILTIN(cycles, cycles_t);

#define CKRING_WARMUP_ITERS 10

#define CKRING_TEST_ITERS 32767

static void
bench_ck_ring(void)
{
	int ret = 0, i;
	cycles_t st, en, tmp;

	rdtscll(tmp);
	rdtscll(st);
	ret = inplace_ring_enq_spsc_cycles((vaddr_t)ipcaddr, (struct ck_ring *)ipcring, &tmp);
	rdtscll(en);
	assert(ret == true);
	ck_enq_min = en - st;
	rdtscll(st);
	ret = inplace_ring_deq_spsc_cycles((vaddr_t)ipcaddr, (struct ck_ring *)ipcring, &tmp);
	rdtscll(en);
	assert(ret == true);
	ck_deq_min = en - st;

	for (i = 0; i < CKRING_TEST_ITERS; i++) {
		rdtscll(st);
		ret = inplace_ring_enq_spsc_cycles((vaddr_t)ipcaddr, (struct ck_ring *)ipcring, &tmp);
		rdtscll(en);
		assert(ret == true);
		if (ck_enq_min > en - st) ck_enq_min = en - st;
	}

	for (i = 0; i < CKRING_TEST_ITERS; i++) {
		rdtscll(st);
		ret = inplace_ring_deq_spsc_cycles((vaddr_t)ipcaddr, (struct ck_ring *)ipcring, &tmp);
		rdtscll(en);
		assert(ret == true);
		if (ck_deq_min > en - st) ck_deq_min = en - st;
	}
}

static void
snd_fn(arcvcap_t r, void *d)
{
	int ret, i;
	cycles_t tmp = sl_now();

	for (i = 0; i < CKRING_WARMUP_ITERS; i++) {
		sl_thd_yield(0);
		inplace_ring_enq_spsc_cycles((vaddr_t)ipcaddr, (struct ck_ring *)ipcring, &tmp);
	}

	snd_ready = 1;
	cos_asnd(sndc, 0);
	while (rcv_ready == 0) ;

	while (likely(test_iters < TEST_ITERS)) {
		unsigned long sndcnt = 0;
		cycles_t en, each_en = 0;

		en = sl_now() + sl_usec2cyc(SNDWINUS);

		each_en = (sl_usec2cyc(SNDWINUS) / SNDRATE);

		for (sndcnt = 0; sndcnt < SNDRATE; sndcnt++) {
			cycles_t st_time;

			cos_rdtsc(st_time);
			ret = inplace_ring_enq_spsc_cycles((vaddr_t)ipcaddr, (struct ck_ring *)ipcring, &st_time);
			assert(ret == true);
			ret = cos_asnd(sndc, 0);
			assert(ret == 0 || ret == -EDQUOT);

			assert(sl_now() < en);
			/* uniform rate. send one.. spin for sometime. */
			while (likely(sl_now() < (st_time + each_en))) ;
		}
//		do {
//			cycles_t st_time;
//
//			if (unlikely(sndcnt == SNDRATE)) break;
//			cos_rdtsc(st_time);
//			ret = inplace_ring_enq_spsc_cycles((vaddr_t)ipcaddr, (struct ck_ring *)ipcring, &st_time);
//			assert(ret == true);
//			ret = cos_asnd(sndc, 0);
//			assert(ret == 0 || ret == -EDQUOT);
//			sndcnt++;
//		} while (likely(sl_now() < en));
//
//		assert(sndcnt == SNDRATE && sl_now() < en);
//		while (likely(sl_now() < en)) ;

		test_iters += sndcnt;
	}

	sl_thd_exit();
}


static void
rcv_fn(arcvcap_t r, void *d)
{
	cycles_t st_time = 0;
	int ret, i;
	unsigned long iters = 0;
	cycles_t begin_time = sl_now(), poll_time = sl_usec2cyc(RCV_POLL_US), poll_abstime = 0, now = 0;

	cos_rcv(r, 0, NULL);
	for (i = 0; i < CKRING_WARMUP_ITERS; i++) {
		sl_thd_yield(0);
		while (inplace_ring_deq_spsc_cycles((vaddr_t)ipcaddr, (struct ck_ring *)ipcring, &st_time) != true) ;
	}
	now = sl_now();
	poll_abstime = now + (poll_time - ((now - begin_time) % poll_time));
	sl_thd_block_timeout(0, poll_abstime);
	//sl_thd_block_timeout(0, sl_now() + sl_usec2cyc(RCV_POLL_US));
	//sl_thd_block_periodic(0);
	rcv_ready = 1;

	while (1) {
		cycles_t poll_timeout = 0, elapsed = 0;
		int pending, rcvd;

		while ((ret = inplace_ring_deq_spsc_cycles((vaddr_t)ipcaddr, (struct ck_ring *)ipcring, &st_time)) == true) {
			test_cycs[iters] = sl_now() - st_time - ck_enq_min - ck_deq_min;

			iters++;
		}

		//assert(iters <= test_iters);
		if (iters >= TEST_ITERS) break;

		/* polling */
		now = sl_now();
		poll_abstime = now + (poll_time - ((now - poll_abstime) % poll_time));
		elapsed = sl_thd_block_timeout(0, poll_abstime);
		//sl_thd_block_timeout(0, sl_now() + sl_usec2cyc(RCV_POLL_US));
		//elapsed = sl_thd_block_periodic(0);
		pending = cos_rcv(r, RCV_ALL_PENDING | RCV_NON_BLOCKING, &rcvd);
		assert(pending == 0 || pending == -EAGAIN);
	}

	printc("\n[%u,%u,%u]\n-------------------------------\n", iters, test_iters, TEST_ITERS);
	for (i = 0; i < ((iters > TEST_ITERS) ? TEST_ITERS : iters); i++) {
		printc("%llu\n", test_cycs[i]);
	}
	printc("-------------------------------\n");

	sl_thd_exit();
}

static volatile int core_ready[NUM_CPU] = { 0 };
void
test_latency_new(void)
{
	int ret = 0, i;
	assert(NUM_CPU == 2);

	PRINTC("Setting up %s\n", cos_cpuid() == SND_CORE ? "sending side" : "receiving side");
	if (cos_cpuid() == SND_CORE) {
		PRINTC("IPI rate: %uipis/%uus, Polling (rcving thd):%uus, Send rate:%usnds/%uus\n", IPI_RATE, IPI_WIN_US, RCV_POLL_US, SNDRATE, SNDWINUS);
		struct sl_thd *st = sl_thd_aep_alloc(snd_fn, NULL, 1, 0, 0, 0);
		assert(st);

		ret = cos_tcap_transfer(sl_thd_rcvcap(st), BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, HI_PRIO);
		assert(ret == 0);
		sl_thd_param_set(st, sched_param_pack(SCHEDP_PRIO, HI_PRIO));

		ipcaddr = (vaddr_t)ipcbuf;
		memset(ipcbuf, 0, IPC_BUF_SZ);
		ipcring = (volatile struct ck_ring *)inplace_ring_init_cycles(ipcaddr, IPC_BUF_SZ);
		assert(ipcring);

		while (rcvc == 0) ;
		sndc = capmgr_asnd_rcv_create(rcvc);
		assert(sndc);
	} else {
		struct sl_thd *rt = sl_thd_aep_alloc(rcv_fn, NULL, 1, 0, IPI_WIN_US, IPI_RATE);
		assert(rt);

		ret = cos_tcap_transfer(sl_thd_rcvcap(rt), BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, HI_PRIO);
		assert(ret == 0);
		sl_thd_param_set(rt, sched_param_pack(SCHEDP_WINDOW, RCV_POLL_US));
		sl_thd_param_set(rt, sched_param_pack(SCHEDP_PRIO, HI_PRIO));

		rcvc = sl_thd_rcvcap(rt);
		while (ipcring == NULL) ;

		bench_ck_ring();
		PRINTC("CK RING MIN DEQ:%llu, ENQ:%llu\n", ck_deq_min, ck_enq_min);
	}
	PRINTC("Done.\n");

	core_ready[cos_cpuid()] = 1;
	for (i = 0; i < NUM_CPU; i++) {
		while (core_ready[i] == 0) ;
	}
}

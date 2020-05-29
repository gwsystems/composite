/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#include <sl.h>
#include <res_spec.h>
#include <hypercall.h>
#include <sched_info.h>
#include <crt_chan.h>
#include <chan_crt.h>
#include <channel.h>

#define MAX_USE_PIPE_SZ 1
#define INITIALIZE_PRIO 1
#define INITIALIZE_PERIOD_MS (4000)
#define INITIALIZE_BUDGET_MS (2000)

static struct sl_thd *__initializer_thd[NUM_CPU] CACHE_ALIGNED;

u32_t cycs_per_usec = 0;
cycles_t *int_start = NULL;
volatile unsigned long *rdy = NULL;

void
sched_child_init(struct sched_childinfo *schedci)
{
	vaddr_t dcbaddr;
	struct sl_thd *initthd;

	assert(schedci);
	assert(!(schedci->flags & COMP_FLAG_SCHED));
	schedci->initthd = sl_thd_initaep_alloc(sched_child_defci_get(schedci), NULL, 0, 0, 0, 0, 0, &dcbaddr);
        assert(schedci->initthd);
	initthd = schedci->initthd;

	sl_thd_param_set(initthd, sched_param_pack(SCHEDP_PRIO, 2));
}

extern void __sched_stdio_thd_init(thdid_t, struct crt_chan *, struct crt_chan *);
#define MAX_PIPE_SZ 8
CRT_CHAN_STATIC_ALLOC(c0, CHAN_CRT_ITEM_TYPE, CHAN_CRT_NSLOTS);
CRT_CHAN_STATIC_ALLOC(c1, CHAN_CRT_ITEM_TYPE, CHAN_CRT_NSLOTS);
CRT_CHAN_STATIC_ALLOC(c2, CHAN_CRT_ITEM_TYPE, CHAN_CRT_NSLOTS);
CRT_CHAN_STATIC_ALLOC(c3, CHAN_CRT_ITEM_TYPE, CHAN_CRT_NSLOTS);
CRT_CHAN_STATIC_ALLOC(c4, CHAN_CRT_ITEM_TYPE, CHAN_CRT_NSLOTS);
CRT_CHAN_STATIC_ALLOC(c5, CHAN_CRT_ITEM_TYPE, CHAN_CRT_NSLOTS);
CRT_CHAN_STATIC_ALLOC(c6, CHAN_CRT_ITEM_TYPE, CHAN_CRT_NSLOTS);
CRT_CHAN_STATIC_ALLOC(c7, CHAN_CRT_ITEM_TYPE, CHAN_CRT_NSLOTS);

#define SPDID_INT 5
#define SPDID_W1  6
#define SPDID_W3  7

#define PRIO_START (MAX_PIPE_SZ + 8)

#define PRIO_INT PRIO_START 
#define PRIO_W0  (PRIO_START - 1)
#define PRIO_W1  (PRIO_START - 2)
#define PRIO_W2  (PRIO_START - 3)
#define PRIO_W3  (PRIO_START - 4)
#define PRIO_W4  (PRIO_START - 5)
#define PRIO_W5  (PRIO_START - 6)
#define PRIO_W6  (PRIO_START - 7)

#define SND_DATA 0x1234

#define SHMCHANNEL_KEY 0x2020
#define MAX_ITERS 100000
cycles_t vals[MAX_ITERS] = { 0 };
int iters = 0;
cycles_t tot = 0, wc = 0;
static int pc, tc;

struct __thd_info {
	struct sl_thd *t;
	tcap_prio_t p;
} iot[MAX_PIPE_SZ + 1];

struct __pipe_info {
	struct sl_thd *sndr, *rcvr; /* p2p channels */
	struct crt_chan *c;
} iop[MAX_PIPE_SZ];

static int
schedinit_self(void)
{
	if (ps_load(&tc) < (MAX_USE_PIPE_SZ + 1)) return 1;

	assert(ps_load(&tc) == (MAX_USE_PIPE_SZ + 1));

	return 0;
}

static void
__init_done(void *d)
{
	while (schedinit_self()) sl_thd_block_periodic(0);

	int i;

	for (i = 0; i < MAX_USE_PIPE_SZ; i++) {
		if (i == 0) {
			crt_chan_init_LU(iop[i].c);
		} else {
			assert(iop[i].sndr && iop[i].rcvr);
			crt_chan_p2p_init_LU(iop[i].c, iop[i].sndr, iop[i].rcvr);
		}
	}

	/* don't want the threads to run before channels are initialized! */
	for (i = MAX_USE_PIPE_SZ; i >= 0; i--) {
		PRINTC("%d, %lx, %u\n", i, (unsigned long)(iot[i].t), sl_thd_thdid(iot[i].t));
		assert(iot[i].t);
		sl_thd_param_set(iot[i].t, sched_param_pack(SCHEDP_PRIO, iot[i].p));
	}
	PRINTLOG(PRINT_DEBUG, "SELF (inc. CHILD) INIT DONE.\n");

	sl_thd_exit();

	assert(0);
}


static void
work_thd_fn(void *data)
{
	int is_last = (int)data;

	ps_faa(rdy, 1);

	while (1) {
		chan_in();
		if (unlikely(is_last)) {
			cycles_t end, diff;
			if (iters >= MAX_ITERS) continue;
			rdtscll(end);
			assert(int_start);
			diff = end - *int_start;
			if (wc < diff) wc = diff;
			tot += diff;
			vals[iters] = diff;
			//printc("%llu\n", diff);
			iters++;
			if (iters % 1000 == 0) printc(".");

			if (iters == MAX_ITERS) {
				int i;

				for (i = 0; i < MAX_ITERS; i++) printc("%llu\n", vals[i]);
				PRINTC("%llu, %llu\n", tot / iters, wc);
				//tot = wc = 0;
				//iters = 0;
			}
			continue;
		}
		chan_out(SND_DATA);
	}
}

thdid_t
sched_child_thd_create(struct sched_childinfo *schedci, thdclosure_index_t idx)
{
	vaddr_t addr;
	struct sl_thd *t = sl_thd_aep_alloc_ext(sched_child_defci_get(schedci), NULL, idx, 0, 0, 0, 0, 0, &addr, NULL);
	assert(t);
	if (cos_inv_token() == SPDID_W1) {
		iot[2].t = t;
		iot[2].p = PRIO_W1;
		iop[1].rcvr = t;
		iop[2].sndr = t;
		__sched_stdio_thd_init(sl_thd_thdid(t), c1, c2);
	} else if (cos_inv_token() == SPDID_W3) {
		iot[4].t = t;
		iot[4].p = PRIO_W3;
		iop[3].rcvr = t;
		__sched_stdio_thd_init(sl_thd_thdid(t), c3, NULL);
	}
	ps_faa(&tc, 1);

	return t ? sl_thd_thdid(t) : 0;
}

thdid_t
sched_child_aep_create(struct sched_childinfo *schedci, thdclosure_index_t idx, int owntc, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax, arcvcap_t *extrcv)
{
	assert(cos_inv_token() == SPDID_INT);
	int first = 1;
	vaddr_t addr;
	/* only 1 aep */
	if (!ps_cas(&first, 1, 0)) assert(0);
	struct sl_thd *t = sl_thd_aep_alloc_ext(sched_child_defci_get(schedci), NULL, idx, 1, owntc, key, ipiwin, ipimax, &addr, extrcv);
	assert(t);
	__sched_stdio_thd_init(sl_thd_thdid(t), NULL, c0);
	iot[0].t = t;
	iot[0].p = PRIO_INT;
	iop[0].sndr = t;
	ps_faa(&tc, 1);

	return t ? sl_thd_thdid(t) : 0;
}

void
test_pipes_init(void)
{
	struct sl_thd *t = sl_thd_alloc(work_thd_fn, MAX_USE_PIPE_SZ == 1 ? (void *)1 : (void *)0);
	assert(t);
	iot[1].t = t;
	iot[1].p = PRIO_W0;
	iop[0].rcvr = t; /* no optimized path for rcving from INT thread */
	iop[1].sndr = t;
	__sched_stdio_thd_init(sl_thd_thdid(t), c0, c1);
	ps_faa(&tc, 1);
	if (MAX_USE_PIPE_SZ >= 3) { 
		t = sl_thd_alloc(work_thd_fn, MAX_USE_PIPE_SZ == 3 ? (void *)1 : (void *)0);
		assert(t);
		iot[3].t = t;
		iot[3].p = PRIO_W2;
		iop[2].rcvr = t;
		iop[3].sndr = t;
		__sched_stdio_thd_init(sl_thd_thdid(t), c2, c3);
		ps_faa(&tc, 1);
	}
}

void
cos_init(void)
{
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci    = cos_compinfo_get(defci);
	static volatile unsigned long first = NUM_CPU + 1, init_done[NUM_CPU] = { 0 };
	static u32_t cpubmp[NUM_CPU_BMP_WORDS] = { 0 };
	int i;

	assert(NUM_CPU == 1);
	assert(MAX_USE_PIPE_SZ <= MAX_PIPE_SZ);
	memset(iop, 0, sizeof(struct __pipe_info) * MAX_PIPE_SZ);
	memset(iot, 0, sizeof(struct __thd_info) * (MAX_PIPE_SZ + 1));
	pc = tc = 0;
	iop[0].c = c0;
	iop[1].c = c1;
	iop[2].c = c2;
	iop[3].c = c3;
	iop[4].c = c4;
	iop[5].c = c5;
	iop[6].c = c6;
	iop[7].c = c7;

	PRINTLOG(PRINT_DEBUG, "CPU cycles per sec: %u\n", cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE));

	if (ps_cas((unsigned long *)&first, NUM_CPU + 1, cos_cpuid())) {
		cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
		cos_defcompinfo_init();
		cos_init_args_cpubmp(cpubmp);
	} else {
		while (!ps_load((unsigned long *)&init_done[first])) ;

		cos_defcompinfo_sched_init();
	}
	ps_faa((unsigned long *)&init_done[cos_cpuid()], 1);

	/* make sure the INITTHD of the scheduler is created on all cores.. for cross-core sl initialization to work! */
	for (i = 0; i < NUM_CPU; i++) {
		if (!bitmap_check(cpubmp, i)) continue;

		while (!ps_load((unsigned long *)&init_done[i])) ;
	}

	sl_init_corebmp(100*SL_MIN_PERIOD_US, cpubmp);
	vaddr_t tscaddr = 0;
	cbuf_t id = channel_shared_page_alloc(SHMCHANNEL_KEY, &tscaddr);
	assert(id > 0);
	int_start = (cycles_t *)tscaddr;
	*int_start = 0ULL;
	rdy = (volatile unsigned long *)(int_start + 1);
	*rdy = 0;
	sched_childinfo_init();
	test_pipes_init();
	__initializer_thd[cos_cpuid()] = sl_thd_alloc(__init_done, NULL);
	assert(__initializer_thd[cos_cpuid()]);
	sl_thd_param_set(__initializer_thd[cos_cpuid()], sched_param_pack(SCHEDP_PRIO, INITIALIZE_PRIO));
	sl_thd_param_set(__initializer_thd[cos_cpuid()], sched_param_pack(SCHEDP_WINDOW, INITIALIZE_BUDGET_MS));
	sl_thd_param_set(__initializer_thd[cos_cpuid()], sched_param_pack(SCHEDP_BUDGET, INITIALIZE_PERIOD_MS));

	hypercall_comp_init_done();

	sl_sched_loop_nonblock();

	PRINTLOG(PRINT_ERROR, "Should never have reached this point!!!\n");
	assert(0);
}

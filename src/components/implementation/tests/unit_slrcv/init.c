#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <llprint.h>
#include <sl.h>
#include <cos_types.h>
#include <cos_dcb.h>
#include <hypercall.h>
#include <schedinit.h>
#include <work.h>
#include <capmgr.h>
#include <crt_chan.h>

static struct sl_xcore_thd *ping;
static struct sl_xcore_thd *pong;

#define HPET_PERIOD_TEST_US 20000

#define WORK_US (1000)

static inline void
ping_fn(void *d)
{
	asndcap_t s = *(asndcap_t *)d;

	while (1) {
		printc("s");
		int r = cos_asnd(s, 0);

		assert(r == 0);
		work_usecs(WORK_US);
	}
	sl_thd_exit();
}

unsigned int iter = 0;
cycles_t st = 0, en = 0, tot = 0, wc = 0;
CRT_CHAN_STATIC_ALLOC(c0, CHAN_CRT_ITEM_TYPE, CHAN_CRT_NSLOTS);
CRT_CHAN_STATIC_ALLOC(c1, CHAN_CRT_ITEM_TYPE, CHAN_CRT_NSLOTS);
CRT_CHAN_STATIC_ALLOC(c2, CHAN_CRT_ITEM_TYPE, CHAN_CRT_NSLOTS);
CRT_CHAN_STATIC_ALLOC(c3, CHAN_CRT_ITEM_TYPE, CHAN_CRT_NSLOTS);
CRT_CHAN_STATIC_ALLOC(c4, CHAN_CRT_ITEM_TYPE, CHAN_CRT_NSLOTS);
CRT_CHAN_STATIC_ALLOC(c5, CHAN_CRT_ITEM_TYPE, CHAN_CRT_NSLOTS);
CRT_CHAN_TYPE_PROTOTYPES(test, int, 4);

#define PIPELINE_LEN 3
#define ITERS 100

static inline void
chrcv(int i)
{
	int r;

	//printc("[r%d]", i);
	switch(i) {
	case 0: crt_chan_recv_test(c0, &r); break;
	case 1: crt_chan_recv_test(c1, &r); break;
	case 2: crt_chan_recv_test(c2, &r); break;
	case 3: crt_chan_recv_test(c3, &r); break;
	case 4: crt_chan_recv_test(c4, &r); break;
	case 5: crt_chan_recv_test(c5, &r); break;
	default: assert(0);
	}
	//printc("[d%d]", i);
}

static inline void
chsnd(int i)
{
	int s = 0xDEAD0000 | i;

	//printc("[s%d]", i);
	switch(i) {
	case 0: crt_chan_send_test(c0, &s); break;
	case 1: crt_chan_send_test(c1, &s); break;
	case 2: crt_chan_send_test(c2, &s); break;
	case 3: crt_chan_send_test(c3, &s); break;
	case 4: crt_chan_send_test(c4, &s); break;
	case 5: crt_chan_send_test(c5, &s); break;
	default: assert(0);
	}
	//printc("[o%d]", i);
}

static inline void
chinit(int i, struct sl_thd *s, struct sl_thd *r)
{
	switch(i) {
	case 0: crt_chan_init_test(c0); break;
	case 1: crt_chan_p2p_init_test(c1, s, r); break;
	case 2: crt_chan_p2p_init_test(c2, s, r); break;
	case 3: crt_chan_p2p_init_test(c3, s, r); break;
	case 4: crt_chan_p2p_init_test(c4, s, r); break;
	case 5: crt_chan_p2p_init_test(c5, s, r); break;
	default: assert(0);
	}
}

static inline void
work_fn(void *x)
{
	int chid = (int)x;
	while (1) {
		chrcv(chid);

		if (likely(chid + 1 < PIPELINE_LEN)) chsnd(chid + 1);
		else {
			rdtscll(en);
			assert(en > st);
			cycles_t diff = en - st;
			if (diff > wc) wc = diff;
			tot += diff;
			iter ++;
			if (unlikely(iter == ITERS)) {
				PRINTC("%llu %llu\n", tot / iter, wc);
				//iter = 0;
				//wc = tot = 0;
			}
		}
	}
	sl_thd_exit();
}

struct sl_thd *wt[PIPELINE_LEN] = { NULL };

static inline void
pong_fn(arcvcap_t r, void *d)
{
	PRINTC("Hpet Register\n");
	int a = capmgr_hw_periodic_attach(HW_HPET_PERIODIC, cos_thdid(), HPET_PERIOD_TEST_US);
	assert(a == 0);

	while (1) {
		if (iter == ITERS) capmgr_hw_detach(HW_HPET_PERIODIC);
		int p = sl_thd_rcv(RCV_ULONLY);
		rdtscll(st);
		chsnd(0);
	}
	sl_thd_exit();
}

void
cos_init(void *d)
{
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo *   ci    = cos_compinfo_get(defci);
	int i;
	static volatile unsigned long init_done[NUM_CPU] = { 0 };
	static volatile arcvcap_t r = 0;
	static volatile asndcap_t s = 0;
	unsigned int cycs_per_us = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);

	if (NUM_CPU == 2) {
		assert(0); // need to rework.. 
		if (cos_cpuid() == 0) {
			cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
			cos_defcompinfo_llinit();
			cos_dcb_info_init_curr();
			sl_init(SL_MIN_PERIOD_US);

			struct sl_thd *t = sl_thd_aep_alloc(pong_fn, NULL, 0, 0, 0, 0);
			assert(t);
			sl_thd_param_set(t, sched_param_pack(SCHEDP_PRIO, TCAP_PRIO_MAX+1));
			r = sl_thd_rcvcap(t);
			assert(r);
		} else {
			while (!ps_load(&init_done[0])) ;

			cos_defcompinfo_sched_init();
			cos_dcb_info_init_curr();
			sl_init(SL_MIN_PERIOD_US);

			struct sl_thd *t = sl_thd_alloc(ping_fn, (void *)&s);
			assert(t);
			sl_thd_param_set(t, sched_param_pack(SCHEDP_PRIO, TCAP_PRIO_MAX+1));

			while (!r) ;
			s = cos_asnd_alloc(ci, r, ci->captbl_cap);
			assert(s);
		}
	} else {
		assert(NUM_CPU == 1);
		cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
		cos_defcompinfo_init();
		sl_init(SL_MIN_PERIOD_US*100);
		int i;
		struct sl_thd *rt = sl_thd_aep_alloc(pong_fn, NULL, 0, 0, 0, 0);
		assert(rt);

		for (i = 0; i < PIPELINE_LEN; i++) {
			wt[i] = sl_thd_alloc(work_fn, (void *)i);
			assert(wt[i]);
			sl_thd_param_set(wt[i], sched_param_pack(SCHEDP_PRIO, TCAP_PRIO_MAX+1+PIPELINE_LEN-i));
			if (i == 0) chinit(i, 0, 0);
			else chinit(i, wt[i-1], wt[i]);
		}

		sl_thd_param_set(rt, sched_param_pack(SCHEDP_PRIO, TCAP_PRIO_MAX+1+PIPELINE_LEN+1));
	}
	ps_faa(&init_done[cos_cpuid()], 1);

	/* make sure the INITTHD of the scheduler is created on all cores.. for cross-core sl initialization to work! */
	for (i = 0; i < NUM_CPU; i++) {
		while (!ps_load(&init_done[i])) ;
	}
	//hypercall_comp_init_done();
	schedinit_child();
	sl_sched_loop();

	PRINTC("Should never get here!\n");
	assert(0);
}

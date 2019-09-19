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

#define HPET_PERIOD_TEST_US 2000

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
CRT_CHAN_STATIC_ALLOC(c0, int, 4);
CRT_CHAN_TYPE_PROTOTYPES(test, int, 4);

static inline void
work_fn(void *x)
{
		int rcv;
	while (1) {
	//	printc("a");
		//sl_thd_block(0);
		crt_chan_recv_test(c0, &rcv);
	//	printc("b");
		rdtscll(en);
		if (unlikely(!st)) continue;
		assert(en > st);
		cycles_t diff = en - st;
		if (diff > wc) wc = diff;
		tot += diff;
		iter ++;
		if (unlikely(iter == 1000)) {
			PRINTC("%llu %llu\n", tot / iter, wc);
			iter = 0;
			wc = tot = 0;
		}
	}
}

struct sl_thd *wt = NULL;
thdid_t wtid = 0;

static inline void
pong_fn(arcvcap_t r, void *d)
{
	//printc("#");
	int a = capmgr_hw_periodic_attach(HW_HPET_PERIODIC, cos_thdid(), HPET_PERIOD_TEST_US);
	assert(a == 0);
	int snd = 0x1234;
	//printc("!");

	int i = 0;
	while (1) {
	//	printc("c");
		int p = sl_thd_rcv(RCV_ULONLY);
		rdtscll(st);
	//	printc("d");
		//int p = cos_rcv(r, 0);

		//printc("[%d] ", i++);
		//sl_thd_wakeup(wtid);
		crt_chan_send_test(c0, &snd);
//		printc("e");
//		rdtscll(en);
//		//if (unlikely(!st)) continue;
//		assert(en > st);
//		cycles_t diff = en - st;
//		if (diff > wc) wc = diff;
//		tot += diff;
//		iter ++;
//		if (unlikely(iter == 1000)) {
//			PRINTC("%llu %llu\n", tot / iter, wc);
//			iter = 0;
//			wc = tot = 0;
//		}

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
		//cos_dcb_info_init_curr();
		sl_init(SL_MIN_PERIOD_US*100);

		crt_chan_init_test(c0);

		wt = sl_thd_alloc(work_fn, NULL);
		assert(wt);
		wtid = sl_thd_thdid(wt);
		struct sl_thd *rt = sl_thd_aep_alloc(pong_fn, NULL, 0, 0, 0, 0);
		assert(rt);
		sl_thd_param_set(wt, sched_param_pack(SCHEDP_PRIO, TCAP_PRIO_MAX+1));
		sl_thd_param_set(rt, sched_param_pack(SCHEDP_PRIO, TCAP_PRIO_MAX+1));
//		r = sl_thd_rcvcap(rt);
//		assert(r);
		//struct sl_thd *st = sl_thd_alloc(ping_fn, (void *)&s);
		//assert(st);
		//sl_thd_param_set(st, sched_param_pack(SCHEDP_PRIO, TCAP_PRIO_MAX+1));

		//s = cos_asnd_alloc(ci, r, ci->captbl_cap);
		//assert(s);
//		s = capmgr_asnd_rcv_create(r);
//		assert(s);
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

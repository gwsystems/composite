#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <llprint.h>
#include <sl.h>
#include <cos_dcb.h>
#include <hypercall.h>

#define MAX_PONG 20
static struct sl_xcore_thd *ping;
static struct sl_xcore_thd *pong[MAX_PONG];

static inline void
ping_fn(void *d)
{
	int k = 0;

	while (1) {
		sl_xcore_thd_wakeup(pong[k % MAX_PONG]);
		k++;
	}
}

static inline void
pong_fn(void *d)
{
	while (1) {
		sl_thd_block(0);
	}
}

void
cos_init(void *d)
{
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo *   ci    = cos_compinfo_get(defci);
	int i;
	static volatile unsigned long first = NUM_CPU + 1, init_done[NUM_CPU] = { 0 };
	static unsigned b1 = 0, b2 = 0, b3 = 0;

	if (ps_cas(&first, NUM_CPU + 1, cos_cpuid())) {
		cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
		cos_defcompinfo_llinit();
	} else {
		while (!ps_load(&init_done[first])) ;

		cos_defcompinfo_sched_init();
	}
	cos_dcb_info_init_curr();
	ps_faa(&init_done[cos_cpuid()], 1);

	/* make sure the INITTHD of the scheduler is created on all cores.. for cross-core sl initialization to work! */
	for (i = 0; i < NUM_CPU; i++) {
		while (!ps_load(&init_done[i])) ;
	}
	sl_init(SL_MIN_PERIOD_US);
	/* barrier, wait for sl_init to be done on all cores */
	ps_faa(&b1, 1);
	while (ps_load(&b1) != NUM_CPU) ;
	if (cos_cpuid()) {
		for (i = 0; i < MAX_PONG; i++) {
			struct sl_thd *t = sl_thd_alloc(pong_fn, NULL);

			assert(t);
			sl_thd_param_set(t, sched_param_pack(SCHEDP_PRIO, TCAP_PRIO_MAX));
			pong[i] = sl_xcore_thd_lookup(sl_thd_thdid(t));
			assert(pong[i]);
		}
	} else {
		struct sl_thd *t = sl_thd_alloc(ping_fn, NULL);

		assert(t);
		sl_thd_param_set(t, sched_param_pack(SCHEDP_PRIO, TCAP_PRIO_MAX));

		ping = sl_xcore_thd_lookup(sl_thd_thdid(t));
		assert(ping);
	}
	ps_faa(&b2, 1);
	while (ps_load(&b2) != NUM_CPU) ;
	PRINTC("Ready!");
//	hypercall_comp_init_done();

	sl_sched_loop_nonblock();

	PRINTC("Should never get here!\n");
	assert(0);
}

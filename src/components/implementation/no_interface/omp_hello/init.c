#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <llprint.h>
#include <sl.h>
#include <cos_omp.h>
#include <cos_dcb.h>

int main(void);

void
cos_exit(int x)
{
	PRINTC("Exit code: %d\n", x);
	while (1) ;
}

static void
cos_main(void *d)
{
	assert(sl_thd_thdid(sl_thd_curr()) == cos_thdid());
	main();

	while (1) ;
}

extern void cos_gomp_init(void);

void
cos_init(void *d)
{
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo *   ci    = cos_compinfo_get(defci);
	int i;
	static volatile unsigned long first = NUM_CPU + 1, init_done[NUM_CPU] = { 0 };
	static unsigned b1 = 0, b2 = 0, b3 = 0;

	PRINTC("In an OpenMP program!\n");
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
	sl_init(SL_MIN_PERIOD_US*100);
	/* barrier, wait for sl_init to be done on all cores */
	ps_faa(&b1, 1);
	while (ps_load(&b1) != NUM_CPU) ;
	cos_gomp_init();
	/* barrier, wait for gomp_init to be done on all cores */
	ps_faa(&b2, 1);
	while (ps_load(&b2) != NUM_CPU) ;

	if (!cos_cpuid()) {
		struct sl_thd *t = NULL;

		t = sl_thd_alloc(cos_main, NULL);
		assert(t);
		sl_thd_param_set(t, sched_param_pack(SCHEDP_PRIO, TCAP_PRIO_MAX));
	}
	/* wait for all cores to reach this point, so all threads wait for main thread to be ready! */
	ps_faa(&b3, 1);
	while (ps_load(&b3) != NUM_CPU) ;

	sl_sched_loop_nonblock();

	PRINTC("Should never get here!\n");
	assert(0);
}


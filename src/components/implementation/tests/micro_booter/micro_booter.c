#include "micro_booter.h"

struct cos_compinfo booter_info;
thdcap_t            termthd[NUM_CPU] = { 0 }; /* switch to this to shutdown */
unsigned long       tls_test[NUM_CPU][TEST_NTHDS];

#include <llprint.h>

/* For Div-by-zero test */
int num = 1, den = 0;

void
term_fn(void *d)
{
	SPIN();
}


extern int cyc_per_usec;
static volatile int test_done[NUM_CPU];

cycles_t rdtscp_min = 0, rdtscp_avg = 0, rdtscp_max = 0;

void
cos_init(void)
{
	int cycs, i;
	static volatile int first_init = 1, init_done = 0;

	cycs = cyc_per_usec = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);
	printc("\t%d cycles per microsecond\n", cycs);

	if (first_init) {
		first_init = 0;

		cos_meminfo_init(&booter_info.mi, BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
		cos_compinfo_init(&booter_info, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
				(vaddr_t)cos_get_heap_ptr(), BOOT_CAPTBL_FREE, &booter_info);

		cos_rdtscp_calib(&rdtscp_min, &rdtscp_avg, &rdtscp_max);
		PRINTC("RDTSCP Calib, MIN:%llu, MAX:%llu, AVG:%llu\n", rdtscp_min, rdtscp_max, rdtscp_avg);
		init_done = 1;
	}

	while (!init_done) ;

	termthd[cos_cpuid()] = cos_thd_alloc(&booter_info, booter_info.comp_cap, term_fn, NULL);
	assert(termthd[cos_cpuid()]);
	PRINTC("Micro Booter started.\n");
	//test_run_mb();

	/* NOTE: This is just to make sense of the output on HW! To understand that microbooter runs to completion on all cores! */
	test_done[cos_cpuid()] = 1;
	for (i = 0; i < NUM_CPU; i++) {
		while (!test_done[i]) ;
	}
	//while (cos_cpuid()) ; /* test only on 1 core! */
	test_run_perf();

	//PRINTC("Micro Booter done.\n");

	cos_thd_switch(termthd[cos_cpuid()]);

	return;
}

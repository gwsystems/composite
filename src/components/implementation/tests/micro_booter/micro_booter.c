#include <cos_types.h>
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

static int test_done[NUM_CPU];

#define COS_DCB_MAX_PER_PAGE (PAGE_SIZE / sizeof(struct cos_dcb_info))
static unsigned long free_off[NUM_CPU] = { 0 }, total[NUM_CPU] = { 0 };
static struct cos_dcb_info *dcb_st[NUM_CPU] = { NULL };

void
cos_dcb_info_init(void)
{
	free_off[cos_cpuid()] = 1;

	dcb_st[cos_cpuid()] = cos_init_dcb_get();
}

struct cos_dcb_info *
cos_dcb_info_get(void)
{
	unsigned int curr_off = 0;

	curr_off = ps_faa(&free_off[cos_cpuid()], 1);
	if (curr_off == COS_DCB_MAX_PER_PAGE) {
		/* will need a version that calls down to capmgr for more pages */
		dcb_st[cos_cpuid()] = cos_dcbpg_bump_allocn(&booter_info, PAGE_SIZE);
		assert(dcb_st[cos_cpuid()]);

		free_off[cos_cpuid()] = 0;

		return dcb_st[cos_cpuid()];
	}

	return (dcb_st[cos_cpuid()] + curr_off);
}

void
cos_init(void)
{
	int cycs, i;
	static int first_init = 1, init_done = 0;
	struct cos_dcb_info *initaddr, *termaddr;

	cycs = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);
	printc("\t%d cycles per microsecond\n", cycs);

	if (first_init) {
		first_init = 0;
		cos_meminfo_init(&booter_info.mi, BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
		cos_compinfo_init(&booter_info, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
				(vaddr_t)cos_get_heap_ptr(), BOOT_CAPTBL_FREE, &booter_info);
		init_done = 1;
	}

	while (!init_done) ;
	cos_dcb_info_init();
	initaddr = cos_init_dcb_get();
	PRINTC("%u DCB IP: %lx, DCB SP: %lx\n", (unsigned int)BOOT_CAPTBL_SELF_INITTHD_CPU_BASE, (unsigned long)cos_introspect(&booter_info, BOOT_CAPTBL_SELF_INITTHD_CPU_BASE, THD_GET_DCB_IP), (unsigned long)cos_introspect(&booter_info, BOOT_CAPTBL_SELF_INITTHD_CPU_BASE, THD_GET_DCB_SP));
	initaddr->ip = 10;
	initaddr->sp = 20;
	PRINTC("%u DCB IP: %lx, DCB SP: %lx\n", (unsigned int)BOOT_CAPTBL_SELF_INITTHD_CPU_BASE, (unsigned long)cos_introspect(&booter_info, BOOT_CAPTBL_SELF_INITTHD_CPU_BASE, THD_GET_DCB_IP), (unsigned long)cos_introspect(&booter_info, BOOT_CAPTBL_SELF_INITTHD_CPU_BASE, THD_GET_DCB_SP));


	termaddr = cos_dcb_info_get();
	termthd[cos_cpuid()] = cos_thd_alloc(&booter_info, booter_info.comp_cap, term_fn, NULL, booter_info.pgtbl_cap, (vaddr_t)termaddr);
	assert(termthd[cos_cpuid()]);
	PRINTC("%u DCB IP: %lx, DCB SP: %lx\n", (unsigned int)termthd[cos_cpuid()], (unsigned long)cos_introspect(&booter_info, termthd[cos_cpuid()], THD_GET_DCB_IP), (unsigned long)cos_introspect(&booter_info, termthd[cos_cpuid()], THD_GET_DCB_SP));
	termaddr->ip = 30;
	termaddr->sp = 40;
	PRINTC("%u DCB IP: %lx, DCB SP: %lx\n", (unsigned int)termthd[cos_cpuid()], (unsigned long)cos_introspect(&booter_info, termthd[cos_cpuid()], THD_GET_DCB_IP), (unsigned long)cos_introspect(&booter_info, termthd[cos_cpuid()], THD_GET_DCB_SP));
	PRINTC("%u DCB IP: %lx, DCB SP: %lx\n", (unsigned int)BOOT_CAPTBL_SELF_INITTHD_CPU_BASE, (unsigned long)cos_introspect(&booter_info, BOOT_CAPTBL_SELF_INITTHD_CPU_BASE, THD_GET_DCB_IP), (unsigned long)cos_introspect(&booter_info, BOOT_CAPTBL_SELF_INITTHD_CPU_BASE, THD_GET_DCB_SP));
	PRINTC("%u DCB IP: %lx, DCB SP: %lx\n", (unsigned int)termthd[cos_cpuid()], (unsigned long)cos_introspect(&booter_info, termthd[cos_cpuid()], THD_GET_DCB_IP), (unsigned long)cos_introspect(&booter_info, termthd[cos_cpuid()], THD_GET_DCB_SP));

	PRINTC("Micro Booter started.\n");
	test_run_mb();

	/* NOTE: This is just to make sense of the output on HW! To understand that microbooter runs to completion on all cores! */
	test_done[cos_cpuid()] = 1;
	for (i = 0; i < NUM_CPU; i++) {
		while (!test_done[i]) ;
	}

	PRINTC("Micro Booter done.\n");

	cos_thd_switch(termthd[cos_cpuid()]);

	return;
}

#include "kernel_tests.h"
#include <init.h>
#include <ps_plat.h>

struct cos_compinfo booter_info;
thdcap_t            termthd[NUM_CPU] = { 0 }; /* switch to this to shutdown */
unsigned long       tls_test[NUM_CPU][TEST_NTHDS];
unsigned long       thd_test[TEST_NTHDS];

#include <llprint.h>

/* For Div-by-zero test */
int num = 1, den = 0;
word_t count = 0;

void
term_fn(void *d)
{
        SPIN();
}

static int test_done[NUM_CPU];

void
cos_init(void)
{
        cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);

	cos_meminfo_init(&booter_info.mi, BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_compinfo_init(&booter_info, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP, 0,
			  (vaddr_t)cos_get_heap_ptr(), BOOT_CAPTBL_FREE, &booter_info);
}

void
test_run_unit_kernel(void)
{
        /* Kernel Tests */
	printc("\n");
        PRINTC("Unit Test Started:\n\n");
        test_timer();
        test_tcap_budgets();
        test_2timers();
        test_thds();
        test_mem_alloc();
        test_async_endpoints();
        test_inv();
        test_captbl_expands();
}

int
main(void)
{
        int i;

        PRINTC("Kernel Tests\n");
        termthd[cos_cpuid()] = cos_thd_alloc(&booter_info, booter_info.comp_cap, term_fn, NULL, 0, 0, 0, 0);
        assert(termthd[cos_cpuid()]);

        cyc_per_usec = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);
	printc("\tTiming: %d cycles per microsecond\n", cyc_per_usec);
	perfcntr_init();
	printc("\tPerformance Counter: %llu\n", ps_tsc());

	test_run_unit_kernel();
        test_run_perf_kernel();

        /* NOTE: This is just to make sense of the output on HW! To understand that microbooter runs to completion on all cores! */
        test_done[cos_cpuid()] = 1;
        for (i = 0; i < NUM_CPU; i++) {
                while (!test_done[i]) ;
        }

        printc("\n");
        PRINTC("Kernel Tests done.\n\n");

        cos_thd_switch(termthd[cos_cpuid()]);

        return 0;
}

void init_done(int par_cont, init_main_t type) {}
void init_exit(int retval) { while (1) ; }

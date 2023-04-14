#include "micro_xcores.h"
#include <llprint.h>
#include <init.h>

struct cos_compinfo booter_info;
thdcap_t            termthd[NUM_CPU] = { 0 }; /* switch to this to shutdown */

/* For Div-by-zero test */
int num = 1, den = 0;
int count = 0;

void
term_fn(void *d)
{
        SPIN();
}

static int test_done[NUM_CPU];

void
cos_init(void)
{
		int cycs, i;
		static int first_init = 1;

        cycs = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);
        printc("\t%d cycles per microsecond\n", cycs);

        if (first_init) {
                first_init = 0;
                cos_meminfo_init(&booter_info.mi, BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
                cos_compinfo_init(&booter_info, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP, 0,
                                      (vaddr_t)cos_get_heap_ptr(), BOOT_CAPTBL_FREE, &booter_info);
        }

        return;
}

void
cos_parallel_init(coreid_t cid, int init_core, int ncores)
{
        termthd[cos_cpuid()] = cos_thd_alloc(&booter_info, booter_info.comp_cap, term_fn, NULL, 0, 0, 0, 0);
        assert(termthd[cos_cpuid()]);
	    return;
}

void
parallel_main(coreid_t cid, int init_core, int ncores)
{
	    int i = 0;

	    test_ipi_switch();
        test_ipi_interference();
        test_ipi_roundtrip();
        test_done[cos_cpuid()] = 1;

	    for (i = 0; i < NUM_CPU; i++) {
		    while (!test_done[i]);
	    }

	    if (cos_cpuid() == 0) PRINTC("Micro Booter Xcore done.\n");

	    cos_thd_switch(termthd[cos_cpuid()]);

	    return;
}

int
main(void)
{
	    assert(0);
	    return 0;
}

void init_done(int par_cont, init_main_t type) {}
void init_exit(int retval) { while (1) ; }

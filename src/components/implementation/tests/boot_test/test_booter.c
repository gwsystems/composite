#include <cos_kernel_api.h>
#include <llprint.h>
#include <cos_types.h>
#include <hypercall.h>
#include <bitmap.h>

void
cos_init(void)
{
	int i = 0;
	u32_t cpubmp[NUM_CPU_BMP_WORDS] = { 0 };

	PRINTLOG(PRINT_DEBUG, "Dummy component booted.\n");
	hypercall_comp_cpubitmap_get(cos_spd_id(), cpubmp);
	PRINTLOG(PRINT_DEBUG, "Comp[%ld] CPU bitmap:", cos_spd_id());
	for (i = NUM_CPU + 1; i >= 0; i--) {
		printc("%d", bitmap_check(cpubmp, i));
	}
	printc("\n");

	hypercall_comp_init_done();

	PRINTLOG(PRINT_ERROR, "Cannot reach here!\n");
	assert(0);
}

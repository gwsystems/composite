#include <cos_kernel_api.h>
#include <cos_types.h>
#include <hw_perf.h>

#define NUM_PAGES 12

word_t buf[NUM_PAGES*PAGE_SIZE];

int
main()
{
	printc("Running pmu test...\n");

	unsigned long dtlb_misses = hw_perf_cnt_dtlb_misses();
	
	for (int i = 0; i < NUM_PAGES; i++) {
		buf[i * PAGE_SIZE] = 0x01;
	}

	dtlb_misses = hw_perf_cnt_dtlb_misses() - dtlb_misses;
	printc("DTLB MISSES %lu\n", dtlb_misses);

	/* the above should print ~roughly~ NUM_PAGES) */

	return 0;
}

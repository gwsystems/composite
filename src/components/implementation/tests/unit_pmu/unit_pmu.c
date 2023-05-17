#include <cos_kernel_api.h>
#include <cos_types.h>
#include <hw_perf.h>

#define NUM_PAGES 12

u8_t buf[NUM_PAGES * PAGE_SIZE];


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


	/* assume our result is correct the number of tlb misses is within 25% 
	 * of the number of pages touched. Always possibilities of architectural
	 * weirdness. This also assumes we are not preempted. 
	 * */
	unsigned long diff = (dtlb_misses > NUM_PAGES) ? dtlb_misses - NUM_PAGES : NUM_PAGES - dtlb_misses;
	
	if (diff <= (0.25 * NUM_PAGES)) {
		printc("SUCCESS\n");
	} else {
		printc("FAILURE\n");
	}

	return 0;
}

#include <cos_kernel_api.h>
#include <cos_types.h>
#include <pong.h>
#include <memmgr.h>

#define NUM_PAGES 100

static unsigned long 
rdpmc (unsigned long cntr)
{
	unsigned int low, high;

	asm volatile("rdpmc" : "=a" (low), "=d" (high) : "c" (cntr));

	return low | ((unsigned long)high) << 32;
}

int
main(void)
{
	cos_pmu_enable_fixed_counter(BOOT_CAPTBL_SELF_INITHW_BASE, 0);
	cos_pmu_enable_fixed_counter(BOOT_CAPTBL_SELF_INITHW_BASE, 1);
	cos_pmu_program_event_counter(BOOT_CAPTBL_SELF_INITHW_BASE, 0, 0x49, 0x0E);
	cos_pmu_program_event_counter(BOOT_CAPTBL_SELF_INITHW_BASE, 1, 0xC5, 0x11);

	unsigned long hw_instructions, core_cycles, dtlb_misses, branch_mispredicts;
	char *buf;
	int  i;

	buf = (char *)memmgr_heap_page_allocn(NUM_PAGES);

	/* write to a bunch of memory */
	for (i = 0; i < NUM_PAGES*PAGE_SIZE; i++) {
		buf[i] = (char)(i % 128);
	}

	dtlb_misses        = rdpmc(0);
	branch_mispredicts = rdpmc(1);
	/* super poorly documented way to read intel's fixed counters */
	hw_instructions    = rdpmc(1<<30);
	core_cycles        = rdpmc((1<<30)+1);

	/* context switch */
	pong_call();

	/* write to a bunch of memory */
	for (i = 0; i < NUM_PAGES*PAGE_SIZE; i++) {
		buf[i] = -(char)(i % 128);
	}

	hw_instructions    = rdpmc(1<<30) - hw_instructions;
	core_cycles        = rdpmc((1<<30)+1) - core_cycles;
	dtlb_misses        = rdpmc(0) - dtlb_misses;
	branch_mispredicts = rdpmc(1) - branch_mispredicts;

	printc("HW Instructions: %lu\n", hw_instructions);
	printc("Core Cycles: %lu\n", core_cycles);
	printc("DTLB Misses: %lu\n", dtlb_misses);
	printc("Branch Mispredicts: %lu\n", branch_mispredicts);

}

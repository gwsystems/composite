
#include <cos_types.h>
#include <hw_perf.h>
#include <cos_kernel_api.h>


/* 
 * x86_64 hardware instruction to read out a performance counter 
 * 
 * cntr = 0->7		: programmable counters 0 through 7 respectivly
 * cntr = 1<<30		: fixed counter 0 (hardware instruction counter on most intel archs)
 * cntr = (1<<30)+1	: fixed counter 1 (core cycles counter on most intel arches) 
 * cntr = (1<<30)+2	: fixed counter 2 (reference cycles counter on most intel arches) 
 *
 * Note: documentation for this is hard to find; I referenced the intel x64 manual and this thread:
 * https://community.intel.com/t5/Software-Tuning-Performance/How-to-read-performance-counters-by-rdpmc-instruction/td-p/1009043
 *
 * */
static inline unsigned long 
rdpmc(unsigned long cntr)
{
	unsigned int low, high;

	asm volatile("rdpmc" : "=a" (low), "=d" (high) : "c" (cntr));

	return low | (((unsigned long)high) << 32);
}

unsigned long
hw_perf_cnt_instructions() 
{
	return rdpmc(1<<30);
}


unsigned long 
hw_perf_cnt_cycles()
{
	return rdpmc((1<<30)+1);
}


unsigned long 
hw_perf_cnt_dtlb_misses()
{
	/* pmc 0 is enabled in the kernel to count dtlb misses */
	return rdpmc(0);
}



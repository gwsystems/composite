#ifndef COS_RDTSC_H
#define COS_RDTSC_H

#include <cos_types.h>

#define COS_RDTSCP_CALIB_ITERS 1000000

#define cos_rdtsc rdtscll

/* Copied from seL4bench */
#define cos_rdtscp(var) do { 					\
	u32_t low, high; 					\
	asm volatile( 						\
			"movl $0, %%eax \n" 			\
			"movl $0, %%ecx \n" 			\
			"cpuid \n" 				\
			"rdtsc \n" 				\
			"movl %%edx, %0 \n" 			\
			"movl %%eax, %1 \n" 			\
			"movl $0, %%eax \n" 			\
			"movl $0, %%ecx \n" 			\
			"cpuid \n" 				\
			: 					\
			"=r"(high), 				\
			"=r"(low) 				\
			: 					\
			: "eax", "ebx", "ecx", "edx" 		\
		    ); 						\
	(var) = (((u64_t)high) << 32ull) | ((u64_t)low); 	\
} while(0)

/*
 * use this to calibrate the rdtscp and perhaps use
 * min value to remove from your benchmarks
 */
static inline void
cos_rdtscp_calib(cycles_t *min, cycles_t *avg, cycles_t *max)
{
	int i;
	volatile cycles_t st, en, mn = 0, mx = 0, total = 0;

	cos_rdtscp(st);
	cos_rdtscp(en);
	mn = mx = en - st;

	for (i = 0; i < COS_RDTSCP_CALIB_ITERS; i++) {
		cycles_t diff;

		cos_rdtscp(st);
		cos_rdtscp(en);

		diff = en - st;
		total += diff;
		if (diff < mn) mn = diff;
		if (diff > mx) mx = diff;
	}

	if (min) *min = mn;
	if (max) *max = mx;
	if (avg) *avg = total / COS_RDTSCP_CALIB_ITERS;

	return;
}

#endif /* COS_RDTSC_H */

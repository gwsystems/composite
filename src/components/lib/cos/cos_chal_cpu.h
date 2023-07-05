#pragma once

#include <cos_types.h>

#ifndef COS_CPU_MHZ
#define COS_CPU_MHZ 2000
#endif

#ifndef rdtscll
#define rdtscll(val) __asm__ __volatile__("rdtsc; shl $32, %%rdx; or %%rdx, %0" : "=a"(val)::"rdx")
#endif

static inline cos_time_t
cos_now(void)
{
	cos_time_t t;

	rdtscll(t);

	return t;
}

static inline cos_cycles_t cos_cycles(void) { return cos_now(); }

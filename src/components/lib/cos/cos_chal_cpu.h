#pragma once

#include <cos_types.h>

#ifndef COS_CPU_MHZ
#define COS_CPU_MHZ 2000
#endif

#ifndef rdtscll
#define rdtscll(val) __asm__ __volatile__("rdtsc; shl $32, %%rdx; or %%rdx, %0" : "=a"(val)::"rdx")
#endif

static inline cos_cycles_t
cos_cycles(void)
{
	cos_cycles_t t;

	rdtscll(t);

	return t;
}

/**
 * `cos_now` returns a time appropriate for the global, absolute
 * timeline. On x86, we have a one-to-one mapping of cycles into the
 * absolute timeline.
 */
static inline cos_time_t
cos_now(void)
{
	return cos_cycles();
}

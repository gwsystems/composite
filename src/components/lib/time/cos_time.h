/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#ifndef COS_TIME_H
#define COS_TIME_H

#include <cos_debug.h>
#include <cos_types.h>
#include <sched.h>

static inline cycles_t
time_cyc_per_usec(void)
{
	static int cycs = 0;

	if (unlikely(cycs <= 0)) cycs = (int)sched_get_cpu_freq();
	assert(cycs > 0);

	return (cycles_t)cycs;
}

static inline microsec_t
time_cyc2usec(cycles_t cyc)
{
	return cyc / time_cyc_per_usec();
}

static inline cycles_t
time_usec2cyc(microsec_t usec)
{
	return usec * time_cyc_per_usec();
}

static inline cycles_t
time_now(void)
{
	cycles_t now;

	rdtscll(now);

	return now;
}

static inline microsec_t
time_now_usec(void)
{
	return time_cyc2usec(time_now());
}

static inline void
time_delay(microsec_t us)
{
	cycles_t until = time_usec2cyc(time_cyc2usec(time_now()) + us);

	/* Unintuitive logic here to consider wrap-around */
	while (until - time_now() < ((cycles_t)0 - (cycles_t)1)) ;

	return;
}

#endif /* COS_TIME_H */

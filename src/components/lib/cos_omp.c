/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2019, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#include <cos_omp.h>
#include <cos_kernel_api.h>
#include <cos_types.h>

static unsigned int _cycs_per_usec = 0;

#define _USEC_TO_SEC_d(x) (((double)x)/(double)(1000*1000))
#define _CYCS_TO_SEC_d(x) _USEC_TO_SEC_d((x)/(double)_cycs_per_usec)

__GOMP_NOTHROW double
omp_get_wtime(void)
{
	cycles_t now;

	rdtscll(now);
	return _CYCS_TO_SEC_d(now);
}

__GOMP_NOTHROW int
omp_get_num_procs(void)
{
	return NUM_CPU;
}

__GOMP_NOTHROW int
omp_get_max_threads(void)
{
	return COS_OMP_MAX_NUM_THREADS;
}

__GOMP_NOTHROW int
omp_get_num_threads(void)
{
	/* FIXME: number of threads in the current team! */
	return omp_get_max_threads();
}

__GOMP_NOTHROW int
omp_get_thread_num(void)
{
	/* 
	 * thread number within a team of a parallel construct! 
	 * master thd will be = 0
	 * not the physical thread id.
	 *
	 * TODO: fetch from team structure?
	 *
	 * For now though, a big hack!
	 */
	return (cos_thdid() % omp_get_max_threads());
}

void
cos_omp_init(void)
{
	_cycs_per_usec = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);

	assert(_cycs_per_usec);
}

/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2019, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#include <part_task.h>
#include <cos_omp.h>
#include <cos_gomp.h>
#include <cos_kernel_api.h>
#include <cos_types.h>

#define COS_OMP_NUM_DEVS 1

static struct cos_icv_global_env       cos_icv_glbenv;
static struct cos_icv_device_env       cos_icv_devenv[COS_OMP_NUM_DEVS];
static struct cos_icv_data_env         cos_icv_init_dataenv;
static struct cos_icv_implicittask_env cos_icv_init_implitskenv;
static unsigned int _cos_omp_init_done = 0;
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
	return COS_GOMP_MAX_THDS;
}

__GOMP_NOTHROW int
omp_get_num_threads(void)
{
	struct sl_thd *t = sl_thd_curr();
	struct part_task *pt = (struct part_task *)t->part_context;

	if (pt) return pt->nthds;

	return 1;
}

__GOMP_NOTHROW int
omp_get_thread_num(void)
{
	struct sl_thd *t = sl_thd_curr();
	struct part_task *pt = (struct part_task *)t->part_context;

	if (!pt) return 0;
	
	return part_task_work_thd_num(pt, PART_CURR_THD);
}

static inline void
cos_omp_icv_global_init(void)
{
	assert(!_cos_omp_init_done);
	/* TODO: what is not int? what is not zero? */
	/* cos_icv_glbenv.xxxx = yyyy; */
}

void
cos_omp_icv_data_init(struct cos_icv_data_env *icvde)
{
	if (unlikely(icvde == &cos_icv_init_dataenv)) {
		assert(!_cos_omp_init_done); /* init only on startup! */

		/* TODO: what is not int? what is not zero! */
		return;
	}

	assert(_cos_omp_init_done);
	memcpy(icvde, &cos_icv_init_dataenv, sizeof(struct cos_icv_data_env));
}

void
cos_omp_icv_implitsk_init(struct cos_icv_implicittask_env *icvite)
{
	if (unlikely(icvite == &cos_icv_init_implitskenv)) {
		assert(!_cos_omp_init_done); /* init only on startup! */

		/* TODO: what is not int? what is not zero! */
		return;
	}

	assert(_cos_omp_init_done);
	memcpy(icvite, &cos_icv_init_implitskenv, sizeof(struct cos_icv_implicittask_env));
}

void
cos_omp_icv_device_init(struct cos_icv_device_env *icvdve, unsigned dev_no)
{
	assert(dev_no < COS_OMP_NUM_DEVS);

	if (unlikely(icvdve == &cos_icv_devenv[dev_no])) {
		assert(!_cos_omp_init_done); /* init only on startup! */

		/* TODO: what is not int? what is not zero! */
		return;
	}

	assert(_cos_omp_init_done);
	memcpy(icvdve, &cos_icv_devenv[dev_no], sizeof(struct cos_icv_device_env));
}

static inline void
cos_omp_icv_init(void)
{
	cos_omp_icv_global_init();

	cos_omp_icv_device_init(&cos_icv_devenv[0], 0);

	cos_omp_icv_data_init(&cos_icv_init_dataenv);
	cos_omp_icv_implitsk_init(&cos_icv_init_implitskenv);
}

void
cos_omp_init(void)
{
	_cycs_per_usec = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);

	assert(_cycs_per_usec);

	cos_omp_icv_init();
	_cos_omp_init_done = 1;
}

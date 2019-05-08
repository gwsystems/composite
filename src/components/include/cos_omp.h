/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2019, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#ifndef COS_OMP_H
#define COS_OMP_H

#include <part_task.h>
#include <cos_types.h>
#include <omp.h>

#define COS_OMP_MAX_NUM_THREADS (PART_MAX_THDS)

struct cos_icv_data_env {
	unsigned dyn_var;
	unsigned nest_var;
	unsigned nthreads_var;
	unsigned run_sched_var;
	unsigned bind_var;
	unsigned thread_limit_var;
	unsigned active_levels_var;
	unsigned levels_var;
	unsigned default_device_var;
};

struct cos_icv_global_env {
	unsigned cancel_var;
	unsigned max_task_priority_var;
};

struct cos_icv_implicittask_env {
	unsigned place_partition_var;
};

struct cos_icv_device_env {
	unsigned def_sched_var;
	unsigned stacksize_var;
	unsigned wait_policy_var;
	unsigned max_active_levels_var;
};

extern void cos_omp_icv_data_init(struct cos_icv_data_env *icvde);
extern void cos_omp_icv_implitsk_init(struct cos_icv_implicittask_env *icvite);
extern void cos_omp_icv_device_init(struct cos_icv_device_env *icvdve, unsigned dev_no);
extern void cos_omp_init(void);

#endif /* COS_OMP_H */

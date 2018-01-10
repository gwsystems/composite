#ifndef COS_THD_CREATION_H
#define COS_THD_CREATION_H

#include <cos_thd_init.h>

#include <res_spec.h>
#include "sched.h"

/* Create a thread in a remote component. This requires an init data
 * entry from that component. Some client thread should alloc the data
 * entry (using cos_thd_init_alloc above) and pass in the index of the
 * entry to the current component. If creation failed or not needed,
 * client thread should use cos_thd_init_free to release. */
static int
cos_thd_create_remote(spdid_t spdid, int idx,
		      u32_t sched_param0, u32_t sched_param1, u32_t sched_param2) {
	int ret;

	if (spdid == cos_spd_id() || idx >= COS_THD_INIT_REGION_SIZE || idx <= 0) return 0;

	ret = sched_create_thd(idx << 16 | spdid, sched_param0, sched_param1, sched_param2);

	return ret;
}

/* Create a thread in the current component. The entry function and
 * data pointer for the new thread are required here. */
static int
cos_thd_create(void *fn, void *data, u32_t sched_param0, u32_t sched_param1, u32_t sched_param2) {
	int idx;
	int spdid = cos_spd_id();

	if (!fn) return 0;

	idx = __init_data_alloc(fn, data);
	assert(idx);
	return sched_create_thd(idx << 16 | spdid, sched_param0, sched_param1, sched_param2);
}

#endif /* COS_THD_CREATION_H */

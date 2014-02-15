#ifndef COS_THD_CREATION_H
#define COS_THD_CREATION_H

#include <cos_types.h>
#include <res_spec.h>
#include <sched.h>
#include <print.h>
#include <cos_debug.h>

#include <ck_pr.h>

extern struct __thd_init_data __thd_init_data[COS_THD_INIT_REGION_SIZE];

static inline int 
__init_data_alloc(void *fn, void *data) {
	int i, ret, tried = 0;

	assert(fn);
AGAIN:	
	for (i = 0; i < COS_THD_INIT_REGION_SIZE; i++) {
		if (__thd_init_data[i].fn == NULL) {
			ret = cos_cas((unsigned long *)&(__thd_init_data[i].fn), (unsigned long)NULL, (unsigned long)fn);
			if (!ret) continue;
			
			assert(__thd_init_data[i].fn == fn);
			__thd_init_data[i].data = data;
			break;
		}
	}

	if (i == COS_THD_INIT_REGION_SIZE) {
		 /* Means no available entry in the data region. */
		if (!tried) {
			/* Try one more time. */
			tried = 1;
			goto AGAIN;
		} else {
			printc("\n COS thread creation: no available entry in init data region, please see comments in cos_thd_creation.h\n");
			printc("Current thd_init_data size: %d\n", COS_THD_INIT_REGION_SIZE);
			BUG();
		}
	}
	
	/* Here we offset the idx by 1 as we use 0 for bootstrap */
	return i + 1;
}

static inline void 
__clear_thd_init_data(int idx) {
	assert(idx > 0 && idx <= COS_THD_INIT_REGION_SIZE
	       && __thd_init_data[idx].fn);
	/* See comments in __init_data_alloc*/
	idx--;
	__thd_init_data[idx].data = NULL;
	/* Memory barrier. We need to ensure that ->data is cleared
	 * before fn. */
	ck_pr_fence_store();
	__thd_init_data[idx].fn = NULL;

	return;
}

/* See comments of cos_thd_create_remote. */
static int 
cos_thd_init_alloc(void *fn, void *data) {
	int idx;

	if (!fn) return -1;

	return idx = __init_data_alloc(fn, data);
}

/* See comments of cos_thd_create_remote. */
static void 
cos_thd_init_free(int idx) {
	/* Release the allocated entry. Usually the new created thread
	 * should clear the entry. So this function only needs to be
	 * called if the thread creation failed for some reason. */
	if (idx > COS_THD_INIT_REGION_SIZE || idx <= 0 
	    || !__thd_init_data[idx].fn) return;

	__clear_thd_init_data(idx);
	
	return;
}

static inline void 
init_data_integration(u32_t *param, int idx) {
	/* Integrate init data to the param. */
	union sched_param param0;

	param0.v = *param;
	param0.c.init_data = idx;
	*param = param0.v;
}

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

	init_data_integration(&sched_param0, idx);
	ret = sched_create_thd(spdid, sched_param0, sched_param1, sched_param2);

	return ret;
}

/* Create a thread in the current component. The entry function and
 * data pointer for the new thread are required here. */
static int 
cos_thd_create(void *fn, void *data, u32_t sched_param0, u32_t sched_param1, u32_t sched_param2) {
	int idx, ret;
	int spdid = cos_spd_id();

	if (!fn) return 0;

	idx = __init_data_alloc(fn, data);
	assert(idx);

	init_data_integration(&sched_param0, idx);
	ret = sched_create_thd(spdid, sched_param0, sched_param1, sched_param2);

	return ret;
}

#endif /* COS_THD_CREATION_H */

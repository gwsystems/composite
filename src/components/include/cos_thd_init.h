#ifndef COS_THD_INIT_H
#define COS_THD_INIT_H

#include <cos_debug.h>

extern struct __thd_init_data __thd_init_data[COS_THD_INIT_REGION_SIZE];

static inline thdclosure_index_t
__init_data_alloc(void *fn, void *data)
{
	int ret, tried = 0;
	thdclosure_index_t i;

	assert(fn);
again:
	for (i = 0; i < COS_THD_INIT_REGION_SIZE; i++) {
		if (__thd_init_data[i].fn == NULL) {
			ret = cos_cas((unsigned long *)&(__thd_init_data[i].fn), (unsigned long)NULL,
			              (unsigned long)fn);
			if (!ret) continue;

			assert(__thd_init_data[i].fn == fn);
			__thd_init_data[i].data = data;
			/* Here we offset the idx by 1 as we use 0 for bootstrap */
			return i + 1;
		}
	}

	/* no available entry in the data region. */
	assert(i == COS_THD_INIT_REGION_SIZE);
	if (!tried) {
		/* Try one more time. */
		tried = 1;
		goto again;
	}
	return -1;
}

static inline void
__clear_thd_init_data(thdclosure_index_t idx)
{
	assert(idx > 0 && idx <= COS_THD_INIT_REGION_SIZE && __thd_init_data[idx].fn);
	idx--; /* See comments in __init_data_alloc*/
	__thd_init_data[idx].data = NULL;
	__thd_init_data[idx].fn   = NULL;

	return;
}

/* See comments of cos_thd_create_remote. */
static thdclosure_index_t
cos_thd_init_alloc(void *fn, void *data)
{
	if (!fn) return -1;
	return __init_data_alloc(fn, data);
}

/*
 * Release the allocated entry. Usually the new created thread should
 * clear the entry. So this function only needs to be called if the
 * thread creation failed for some reason.
 */
static void
cos_thd_init_free(thdclosure_index_t idx)
{
	if (idx > COS_THD_INIT_REGION_SIZE || idx <= 0 || !__thd_init_data[idx].fn) return;

	__clear_thd_init_data(idx);

	return;
}

#endif /* COS_THD_INIT_H */

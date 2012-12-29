#ifndef COS_SCHED_DS_H
#define COS_SCHED_DS_H

#include <cos_types.h>
struct cos_sched_data_area cos_sched_notifications __attribute__((section(".kmem"))) = {
	.cos_next = {.next_thd_id = 0, .next_thd_flags = 0},
	.cos_locks = {.v = 0},
	.cos_events = {}
};

#endif

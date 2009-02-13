/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <cos_component.h>

/* 
 * This is initialized at load time with the spd id of the current
 * spd, and is passed into all system calls to identify the calling
 * service.
 */
long cos_this_spd_id = 0;
void *cos_heap_ptr = NULL;
struct cos_sched_data_area cos_sched_notifications = {
	.cos_next = {.next_thd_id = 0, .next_thd_flags = 0, .next_thd_urgency = 0},
	.cos_locks = {.owner_thd = 0, .queued_thd = 0},
	.cos_events = {}
};

__attribute__ ((weak))
void cos_init(void *arg)
{
	return;
}

__attribute__ ((weak))
void cos_upcall_exec(void *arg)
{
	return;
}

__attribute__ ((weak))
void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	switch (t) {
	case COS_UPCALL_BRAND_EXEC:
	{
		cos_upcall_exec(arg1);
		break;
	}
	case COS_UPCALL_BOOTSTRAP:
	{
		cos_argreg_init();
		cos_init(arg1);
		break;
	}
	default:
		*(int*)NULL = 0;
		return;
	}
	return;
}

__attribute__ ((weak))
int main(void)
{
	return 0;
}

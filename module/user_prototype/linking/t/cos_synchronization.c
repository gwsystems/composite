/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include "cos_component.h"
#include "cos_synchronization.h"

extern int COS_EXTERN_FN(lock_component_take)(spdid_t spd, unsigned short int thd, 
					  unsigned short int gen, unsigned long lock_id);
extern int COS_EXTERN_FN(lock_component_release)(spdid_t spd, unsigned short int gen, unsigned long lock_id);
extern unsigned long COS_EXTERN_FN(lock_component_alloc)(spdid_t spd);
extern unsigned long COS_EXTERN_FN(lock_component_free)(spdid_t spd, unsigned long lock_id);

void cos_lock_take(cos_lock_t *l)
{
	unsigned short int thd, gen;
	unsigned short int curr_thd = (unsigned short int)cos_get_thd_id();

	do {
		__asm__ __volatile__("call cos_atomic_user1"
				     : "=c" (gen), "=D" (thd) 
				     : "a" (l), "b" (curr_thd)
				     : "edx", "memory", "cc");
		/* I'm not trusting gcc to clobber registers it should
		 * (see cos_component.h and the syscall defines) */
		__asm__ __volatile__("":::"ecx", "edi", "eax", "ebx");

		/* If another thread holds the lock, notify lock component */
		if (0 != thd) {
			if (COS_EXTERN_FN(lock_component_take)(cos_spd_id(), 0x7FFF & thd, gen, l->lock_id)) {
				*(unsigned long*)NULL = 0; // die as we have a serious error
			}
		}
	} while (0 != thd);

	return;
}

void cos_lock_release(cos_lock_t *l)
{
	unsigned short int gen, contention;
	unsigned short int curr_thd = (unsigned short int)cos_get_thd_id();

	__asm__ __volatile__("call cos_atomic_user2"
			     : "=c" (gen), "=D" (contention)
			     : "a" (l), "b" (curr_thd)
			     : "memory", "cc");
	__asm__ __volatile__("":::"ecx", "edi", "eax", "ebx");
	/* release those threads waiting on the lock by updating current gen */
	if (contention) {
		COS_EXTERN_FN(lock_component_release)(cos_spd_id(), gen, l->lock_id);
	}

	return;
}

int cos_lock_init(cos_lock_t *l)
{
	l->take_thd = 0;
	l->generation = 0;
	l->lock_id = COS_EXTERN_FN(lock_component_alloc)(cos_spd_id());
	if (l->lock_id == 0) {
		return -1;
	}
	return 0;
}

void cos_lock_cleanup(cos_lock_t *l)
{
	COS_EXTERN_FN(lock_component_free)(cos_spd_id(), l->lock_id);

	return;
}

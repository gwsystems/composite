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
volatile long cos_this_spd_id = 0;
void *cos_heap_ptr;

__attribute__ ((weak))
void cos_upcall_fn(vaddr_t data_region, int thd_id, 
		   void *arg1, void *arg2, void *arg3)
{
	return;
}

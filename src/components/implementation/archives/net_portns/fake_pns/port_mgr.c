/**
 * Copyright 2009 by Boston University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gabep1@cs.bu.edu, 2009
 */

/* 
 * This is a place-holder for the manager of the port namespace.  
 */
#include <cos_component.h>
#include <cos_synchronization.h>

#include <net_portns.h>

cos_lock_t port_lock;
int lock_initialized = 0;

/* need a rodata section */
const char *name = "port_mgr";

int portmgr_new(spdid_t spdid)
{
	if (!lock_initialized) {
		lock_initialized = 1;
		lock_static_init(&port_lock);
	}

	return 1;
}

int portmgr_bind(spdid_t spdid, u16_t port)
{
	if (!lock_initialized) {
		lock_initialized = 1;
		lock_static_init(&port_lock);
	}

	return 0;
}

void portmgr_free(spdid_t spdid, u16_t port)
{
	return;
}

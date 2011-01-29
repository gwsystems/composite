/**
 * Copyright 2011 by The George Washington University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gparmer@gwu.edu, 2011
 */

#include <cos_component.h>
#include <pgfault.h>
#include <sched.h>
#include <print.h>

void fault_page_fault_handler(spdid_t spdid, void *fault_addr, int flags, void *ip)
{
	printc("Fault in spd %d @ %p while executing at %p\n", spdid, fault_addr, ip);
	flags = *((int*)0);
	sched_block(spdid, 0);
	return;
}

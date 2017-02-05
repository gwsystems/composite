/**
 * Copyright 2009 by Gabriel Parmer, gparmer@gwu.edu.  All rights
 * reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <cos_component.h>
#include <print.h>

#include <timed_blk.h>
#include <sched.h>

void cos_init(void *arg)
{
	while (1) {
		//printc("--blocking-->\n");
		timed_event_block(cos_spd_id(), 100);
	}
	return;
}

void bin (void)
{
	sched_block(cos_spd_id(), 0);
}

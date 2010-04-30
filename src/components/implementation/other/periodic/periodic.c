/**
 * Copyright 2009 by Gabriel Parmer, gparmer@gwu.edu.  All rights
 * reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <cos_component.h>
#include <print.h>

#include <periodic_wake.h>
#include <sched.h>
#include <sched_conf.h>

void cos_init(void *arg)
{
	periodic_wake_create(cos_spd_id(), 5);
	while (1) {
		printc("--wakeup @ %ld -->\n", sched_timestamp());
		periodic_wake_wait(cos_spd_id());
	}
	return;
}

void bin (void)
{
	sched_block(cos_spd_id(), 0);
}

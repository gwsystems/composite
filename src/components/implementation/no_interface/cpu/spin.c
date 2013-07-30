/**
 * Copyright 2009 by Gabriel Parmer, gparmer@gwu.edu.  All rights
 * reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <stdlib.h>
#include <cos_component.h>
#include <print.h>

#include <sched.h>
#include <periodic_wake.h>

static unsigned int period = 0, budget = 0, execution = 0, priority = 16;

static void
spin(int execution)
{
	/* This will require calibration */
	BUG();
	return;
}

static void
execute(void)
{
	if (period && execution) {
		if (periodic_wake_create(cos_spd_id(), period) < 0) {
			printc("spdid %d: could not create periodic timer.\n", 
			       (int)cos_spd_id());
			return;
		}
	}

	printc("%d: execution beginning in cpu component.\n", 
	       (unsigned int)cos_spd_id());

	while (1) {
		if (period && execution) periodic_wake_wait(cos_spd_id());
		if (execution) {
			spin(execution);
		} 
	}
}

#include <cos_sparams_parse.h>

void 
cos_init(void *arg)
{
	static int first = 1;

	if (first) {
		union sched_param sp[SCHED_PARAM_MAX];
		int i;

		first = 0;
		parse_sched_str(cos_init_args(), sp, SCHED_PARAM_MAX);
		for (i = 0 ; i < SCHED_PARAM_MAX ; i++) {
			switch (sp[i].c.type) {
			case SCHEDP_PRIO: 
				priority = sp[i].c.value; 
				break;
			case SCHEDP_WINDOW: 
				period   = sp[i].c.value; 
				break;
			case SCHEDP_BUDGET: 
				budget   = sp[i].c.value; 
				break;
			}
		}
		if (sched_create_thd(cos_spd_id(), sp[0].v, sp[1].v, sp[2].v) < 0) {
			printc("spdid %d: could not create thread.\n", (int)cos_spd_id());
		}
	} else {
		execute();
	}

	return;
}

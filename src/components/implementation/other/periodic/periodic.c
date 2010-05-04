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

#include <periodic_wake.h>
#include <sched.h>
#include <sched_conf.h>
#include <synth_hier.h>

int parse_initstr(void)
{
	struct cos_array *data;
	char *c;
	int period;

	data = cos_argreg_alloc(sizeof(struct cos_array) + 52);
	assert(data);
	data->sz = 52;
	
	if (sched_comp_config_initstr(cos_spd_id(), data)) {
		printc("No initstr found.\n");
		return -1;
	}

	c = data->mem;
	period = atoi(c);
	
	cos_argreg_free(data);

	return period;
}

void cos_init(void *arg)
{
	int period;

	period = parse_initstr();
	if (period < 1) BUG();
	periodic_wake_create(cos_spd_id(), period);
	while (1) {
		left();
		periodic_wake_wait(cos_spd_id());
	}
	return;
}

void bin (void)
{
	sched_block(cos_spd_id(), 0);
}

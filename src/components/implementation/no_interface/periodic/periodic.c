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
#include <synth_hier.h>

int period = 100, num_invs = 1;

char *parse_step(char *d)
{
	char *s = strchr(d, ' ');
	if (!s) {
		if ('\0' == d) return d;
		s = d + strlen(d);
	} else {
		*s = '\0';
		s++;
	}

	switch(*d) {
	case 'p':		/* spin */
		period = atoi(++d);
		break;
	case 'n':		/* num of invocations */
		num_invs = atoi(++d);
		break;
	}

	return s;
}

int parse_initstr(void)
{
//	struct cos_array *data;
	char *c;

	c = cos_init_args();
	while ('\0' != *c) c = parse_step(c);

	/* data = cos_argreg_alloc(sizeof(struct cos_array) + 52); */
	/* assert(data); */
	/* data->sz = 52; */
	
	/* if (sched_comp_config_initstr(cos_spd_id(), data)) { */
	/* 	printc("No initstr found.\n"); */
	/* 	return -1; */
	/* } */

	/* c = data->mem; */
	/* while ('\0' != *c) c = parse_step(c); */
	
	/* cos_argreg_free(data); */

	return 0;
}

void cos_init(void *arg)
{
	parse_initstr();
	if (period < 1) BUG();
	periodic_wake_create(cos_spd_id(), period);

	printc("Periodic task %d ready to rock with period %d\n", cos_get_thd_id(), period);

	/* Allow all periodic tasks to begin */
	periodic_wake_wait(cos_spd_id());
	periodic_wake_wait(cos_spd_id());
	periodic_wake_wait(cos_spd_id());
	while (1) {
		int i;

		for (i = 0 ; i < num_invs ; i++) left();
		periodic_wake_wait(cos_spd_id());
	}
	return;
}

void bin (void)
{
	sched_block(cos_spd_id(), 0);
}

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

static char *
parse_step(char *d)
{
	char *s;

	if (d == '\0') return d;
	s = strchr(d, ' ');
	if (!s) {
		s = d + strlen(d);
	} else {
		*s = '\0';
		s++;
	}

	switch(*d) {
	case 'p':
		period    = atoi(++d);
		break;
	case 'b':
		budget    = atoi(++d);
		break;
	case 'e':
		execution = atoi(++d);
		printc("WARNING %d: constraining execution time not implemented\n", 
		       (int)cos_spd_id());
		break;
	case 'a':
		priority  = atoi(++d);
		break;
	default:
		printc("periodic spin %d: could not parse argument %s\n", 
		       (int)cos_spd_id(), d);
		break;
	}

	return s;
}

static void
parse_initstr(void)
{
	char *c;

	c = cos_init_args();
	while ('\0' != *c) c = parse_step(c);
}

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

	while (1) {
		if (period && execution) periodic_wake_wait(cos_spd_id());
		if (execution) {
			spin(execution);
		} 
		if (!period && !execution) {
			while (1) ;
		}
	}
}

void 
cos_init(void *arg)
{
	static int first = 1;

	if (first) {
		union sched_param sp[3];
		int off = 0, i;

		first = 0;
		parse_initstr();
		for (i = 0 ; i < 3 ; i++) {
			sp[i].c.type = SCHEDP_NOOP;
		}
		
		if (priority) {
			union sched_param *p;
			p          = &sp[off++];
			p->c.type  = SCHEDP_PRIO;
			p->c.value = priority;
		}
		if (budget) {
			union sched_param *p;
			p          = &sp[off++];
			p->c.type  = SCHEDP_BUDGET;
			p->c.value = budget;
		}
		if (period) {
			union sched_param *p;
			p          = &sp[off++];
			p->c.type  = SCHEDP_WINDOW;
			p->c.value = period;
		}

		if (sched_create_thd(cos_spd_id(), sp[0].v, sp[1].v, sp[2].v) < 0) {
			printc("spdid %d: could not create thread.\n", (int)cos_spd_id());
		}
	} else {
		execute();
	}

	return;
}

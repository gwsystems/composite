/**
 * Copyright 2013 by Gabriel Parmer, gparmer@gwu.edu.  All rights
 * reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <cos_component.h>
#include <print.h>
#include <stdlib.h>

#include <sched.h>

static char *
parse_step(char *d, int *val)
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
	*val = atoi(d);

	return s;
}

static int 
boot_spd_thd(spdid_t spdid)
{
	union sched_param sp = {.c = {.type = SCHEDP_RPRIO, .value = 1}};

	/* Create a thread IF the component requested one */
	if ((sched_create_thread_default(spdid, sp.v, 0, 0)) < 0) return -1;
	return 0;
}

static void
parse_process_initstr(void)
{
	char *c;

	c = cos_init_args();
	while ('\0' != *c) {
		int spdid = 0;
		
		c = parse_step(c, &spdid);
		if (!spdid) continue;
		boot_spd_thd(spdid);
	}
}

void 
cos_init(void *arg)
{
	parse_process_initstr();

	return;
}

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
#include <sconf/sconf.h>

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
	int i, l;
	char *c;
	struct sconf sc;
	sconftok_t ts[32];
	sconf_ret_t r = SCONF_SUCCESS;

	c = cos_init_args();
	l = strlen(c);
	sconf_init(&sc, c, l, ts, 32);
	sconf_parse(&sc);
	for (i = 0 ; r == SCONF_SUCCESS ; i++) {
		int spdid;
		
		r = sconf_arr_int(&sc, i, &spdid);
		if (r != SCONF_SUCCESS) break;
		boot_spd_thd(spdid);
	}
}

void 
cos_init(void *arg)
{
	parse_process_initstr();

	return;
}

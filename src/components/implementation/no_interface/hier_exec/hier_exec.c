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
	int i;
	char *c;
	struct sconf sc;
	sconftok_t ts[32];
	sconf_ret_t r = SCONF_SUCCESS;

	c = cos_init_args();
	sconf_init(&sc, c, strlen(c)+1, ts, 32);
	if (sconf_parse(&sc) < 0) {
		printc("%d: parsing %s unsuccessful\n", 
		       (unsigned int)cos_spd_id(), c);
		return;
	}
	for (i = 0 ; 1 ; i++) {
		int spdid;

		printc("token %d->%d, type %d\n", sc.tok_start, sc.tok_end, sc.type);
		r = sconf_arr_int(&sc, i, &spdid);
		printc("Array parse returns %d\n", r);
		if (r != SCONF_SUCCESS) break;
		printc("Creating thread in %d\n", spdid);
		boot_spd_thd(spdid);
	}
}

void 
cos_init(void *arg)
{
	printc("In the hierarchical execution component\n");
	parse_process_initstr();
	printc("Completing execution in the hierarchical execution component\n");

	return;
}

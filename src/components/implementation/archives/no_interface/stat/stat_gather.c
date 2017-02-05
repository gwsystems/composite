/**
 * Copyright 2009 by Gabriel Parmer, gabep1@cs.bu.edu.  All rights
 * reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#define COS_FMT_PRINT

#include <cos_component.h>
#include <cos_debug.h>
#include <print.h>

#define STAT_FREQ 100
//#define STATS_COLLECT

#include <timed_blk.h>
#include <lock.h>
#include <evt.h>
#include <sched.h>

typedef unsigned long *(*stat_fn)(spdid_t, unsigned long *);
typedef int (*stat_len_fn)(spdid_t);

struct stat_fns {
	char *name;
	stat_fn stat;
	stat_len_fn stat_len;
};

static struct stat_fns *holder, client_comp_fns[] = {
	{ .name = "Lock stats:", .stat = lock_stats, .stat_len = lock_stats_len },
	{ .name = "Evt stats:", .stat = evt_stats, .stat_len = evt_stats_len },
	{ .name = NULL, .stat = NULL, .stat_len = NULL }
};

void gather_stats(char *name, stat_fn f, stat_len_fn l)
{
	unsigned long *ss, *ret;
	int len;

	printc("%s\n", name);
	len = l(cos_spd_id());
	if (len == 0) return;
	ss = cos_argreg_alloc(len * sizeof(unsigned long));
	assert(ss);
	while (NULL != (ret = f(cos_spd_id(), ss))) {
		int i;

		assert(ret == ss);
		for (i = 0 ; i < len ; i++) {
			printc("\t%lu", ss[i]);
		}
		prints("\n");
	}
	cos_argreg_free(ss);
}

void cos_init(void *arg)
{
#ifdef STATS_COLLECT
	while (1) {
		int i = 0;

		timed_event_block(cos_spd_id(), STAT_FREQ);
		for (i = 0 ; client_comp_fns[i].name != NULL ; i++) {
			gather_stats(client_comp_fns[i].name, client_comp_fns[i].stat,
				     client_comp_fns[i].stat_len);
		}
	}
#endif
	return;
}

void bin (void)
{
	holder = client_comp_fns;
	sched_block(cos_spd_id(), 0);
}

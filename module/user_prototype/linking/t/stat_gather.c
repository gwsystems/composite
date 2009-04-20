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

#define STAT_FREQ 500

extern int timed_event_block(spdid_t spdid, unsigned int microsec);

extern unsigned long *lock_stats(spdid_t spdid, unsigned long *s);
extern int lock_stats_len(spdid_t spdid);
//extern unsigned long *evt_stats(unsigned long *s);
//extern int evt_stats_len(void);

typedef unsigned long *(*stat_fn)(spdid_t, unsigned long *);
typedef int (*stat_len_fn)(spdid_t);

void gather_stats(char *name, stat_fn f, stat_len_fn l)
{
	unsigned long *ss, *ret;
	int len;

	printc("%s\n", name);
	len = l(cos_spd_id());
	if (len == 0) return;
	assert(len > 0 && len < COS_MAX_ARG_SZ);
	ss = cos_argreg_alloc(len * sizeof(unsigned long));
	assert(ss);
	while (NULL != (ret = f(cos_spd_id(), ss))) {
		int i;

		assert(ret == ss);
		for (i = 0 ; i < len ; i++) {
			printc("\t%ld", ss[i]);
		}
		prints("\n");
	}
	cos_argreg_free(ss);
}

void cos_init(void *arg)
{
	while (1) {
		timed_event_block(cos_spd_id(), STAT_FREQ);
		gather_stats("Lock stats:", lock_stats, lock_stats_len);
//		gather_stats("Evt stats:", evt_stats, evt_stats_len);
	}
	return;
}

void bin (void)
{
	extern int sched_block(spdid_t spdid);
	sched_block(cos_spd_id());
}

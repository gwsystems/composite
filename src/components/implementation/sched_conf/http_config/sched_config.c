/**
 * Copyright 2009, Gabriel Parmer, The George Washington University,
 * gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <string.h>

#include <cos_component.h>
#include <cos_debug.h>

#include <sched_conf.h>

struct component_init_str {
	unsigned int spdid, schedid;
	char init_str[56];
};

struct component_init_str *init_strs;

static void parse_initialization_strings(void)
{
	int i;

	init_strs = (struct component_init_str*)((char*)cos_heap_ptr-PAGE_SIZE);
	for (i = 1 ; init_strs[i].spdid ; i++) ;
}

/* Get the nth component for this specific scheduler */
struct component_init_str *nth_for_sched(spdid_t sched, int n)
{
	int i, idx;

	for (i = 0, idx = 1 ; i <= n ; i++, idx++) {
		if (0 == init_strs[idx].spdid) return &init_strs[idx];
		/* bypass other scheduler's components */
		while (init_strs[idx].schedid != sched && init_strs[idx].spdid != 0) {
			idx++;
		}
		if (i == n) return &init_strs[idx];
	}
	assert(0);
	return &init_strs[idx-1];
}

spdid_t sched_comp_config(spdid_t spdid, int i, struct cos_array *data)
{
	static int first = 1;
	int max_len, str_len;
	struct component_init_str *cis;

	if (first) {
		first = 0;
		parse_initialization_strings();
		printc("init...\n");
	}

	if (!cos_argreg_arr_intern(data)) {
		assert(0); 
		return 0;
	}
	max_len = data->sz;

	cis = nth_for_sched(spdid, i);
	if (0 == cis->spdid) return 0; /* no dice */
	assert(cis->schedid == spdid);

	str_len = strlen(cis->init_str);
	if (str_len+1 > max_len) {
		assert(0); 
		return 0;
	}

	strcpy(data->mem, cis->init_str);
	data->sz = str_len;

	return cis->spdid;
}

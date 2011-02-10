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
	int startup;
	char init_str[52];
}__attribute__((packed));

struct component_init_str *init_strs;

static void parse_initialization_strings(void)
{
	int i;

	init_strs = (struct component_init_str*)((char*)cos_get_heap_ptr()-PAGE_SIZE);
	for (i = 1 ; init_strs[i].spdid ; i++) ;
	//printc("initialization string for %d is %s\n", init_strs[i].spdid, init_strs[i].init_str);
}

/* Get the nth component for this specific scheduler */
static struct component_init_str *nth_for_sched(spdid_t sched, int n, int startup)
{
	int i, idx;

	for (i = 0, idx = 1 ; i <= n ; i++, idx++) {
		if (0 == init_strs[idx].spdid) return &init_strs[idx];
		/* bypass other scheduler's components, and those not
		 * started on bootup */
		while (init_strs[idx].schedid != sched ||
		       startup == init_strs[idx].startup) {
			if (0 == init_strs[idx].spdid) return &init_strs[idx];
			idx++;
		}

		if (i == n) return &init_strs[idx];
	}
	BUG();
	return &init_strs[idx-1];
}

static struct component_init_str *init_str_for_spdid(spdid_t spdid)
{
	int i;

	for (i = 1 ; init_strs[i].spdid ; i++) {
		if (spdid == init_strs[i].spdid) return &init_strs[i];
	}
	return NULL;
}

static struct component_init_str *spd_for_sched(spdid_t sched, spdid_t target)
{
	int i;

	for (i = 1 ; init_strs[i].spdid != 0 ; i++) {
		if (init_strs[i].schedid == sched &&
		    init_strs[i].spdid   == target) {
			return &init_strs[i];
		}
	}
	return &init_strs[i];
}

int first = 1;

static int params_initstr(char *s, char **start, int *sz)
{
	char *b = strchr(s, (int)'\''), *e;
	
	if (NULL == b) {
		*start = NULL;
	} else {
		b++;
		e = strchr(b, (int)'\'');
		*start = b;
		*sz = e-b;
	}
	return 0;
}

int sched_comp_config_initstr(spdid_t spdid, struct cos_array *data)
{
	int max_len, str_len = 0;
	struct component_init_str *cis;
	char *s;

	if (first) {
		first = 0;
		parse_initialization_strings();
	}
	if (!cos_argreg_arr_intern(data)) {
		BUG(); 
		return -1;
	}
	max_len = data->sz;

	cis = init_str_for_spdid(spdid);
	if (NULL == cis) return -1; /* no dice */

	params_initstr(cis->init_str, &s, &str_len);
	if (str_len+1 > max_len) {
		BUG(); 
		return -1;
	}

	memcpy(data->mem, s, str_len);
	data->mem[str_len] = '\0';


	data->sz = str_len;

	return 0;
}

static int params_sched(char *s, char **start, int *sz)
{
	char *c = strchr(s, (int)'\'');

	if (!c) {
		*start = s;
		*sz = strlen(s);
	} else {
		*start = s;
		*sz = c-s;
	}

	return 0;
}

int sched_comp_config_default(spdid_t spdid, spdid_t target, struct cos_array *data)
{
	int max_len, str_len;
	struct component_init_str *cis;
	char *s;

	if (first) {
		first = 0;
		parse_initialization_strings();
	}
	if (!cos_argreg_arr_intern(data)) {
		BUG(); 
		return -1;
	}
	max_len = data->sz;

	cis = spd_for_sched(spdid, target);
	if (0 == cis->spdid) return -1; /* no dice */
	assert(cis->schedid == spdid);

	params_sched(cis->init_str, &s, &str_len);
	//str_len = strlen(cis->init_str);
	if (str_len+1 > max_len) {
		BUG(); 
		return -1;
	}
	if (str_len == 0) return 1;

	memcpy(data->mem, s, str_len);
	data->mem[str_len] = '\0';
	//strcpy(data->mem, cis->init_str);
	data->sz = str_len;

	return 0;
}

spdid_t
__sched_comp_config(spdid_t spdid, int i, struct cos_array *data, int dyn_loaded)
{
	int max_len, str_len;
	struct component_init_str *cis;
	char *s;

	if (first) {
		first = 0;
		parse_initialization_strings();
	}

	if (!cos_argreg_arr_intern(data)) {
		BUG(); 
		return 0;
	}
	max_len = data->sz;

	cis = nth_for_sched(spdid, i, dyn_loaded);
	if (0 == cis->spdid) return 0; /* no dice */
	assert(cis->schedid == spdid);

	params_sched(cis->init_str, &s, &str_len);
	//str_len = strlen(cis->init_str);
	if (str_len+1 > max_len) {
		BUG(); 
		return -1;
	}

	memcpy(data->mem, s, str_len);
	data->mem[str_len] = '\0';
	//strcpy(data->mem, cis->init_str);
	data->sz = str_len;

	return cis->spdid;
}

spdid_t
sched_comp_config(spdid_t spdid, int i, struct cos_array *data)
{
	return __sched_comp_config(spdid, i, data, 0);
}

spdid_t
sched_comp_config_poststart(spdid_t spdid, int i, struct cos_array *data)
{
	return __sched_comp_config(spdid, i, data, 1);
}

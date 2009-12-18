/**
 * Copyright 2009, Gabriel Parmer, The George Washington University,
 * gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <cos_component.h>
#include <cos_debug.h>

#include <sched_config.h>

#include <string.h>
spdid_t sched_comp_config(spdid_t spdid, int index, struct cos_array *data)
{
	int max_len, str_len;
	struct comp_sched_info *csi;

	if (!cos_argreg_arr_intern(data)) {assert(0); return 0;}
	max_len = data->sz;
	if (index+1 > MAX_COMP_IDX) return 0;

	csi = &comp_info[index+1];
	str_len = strlen(csi->sched_str);
	if (str_len+1 > max_len) {assert(0); return 0;}

	strcpy(data->mem, csi->sched_str);
	data->sz = str_len;
	
	return csi->spdid;
}

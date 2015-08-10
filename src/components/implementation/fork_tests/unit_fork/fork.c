/**
 * Copyright 2015 by Gedare Bloom gedare@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <cos_component.h>
#include <print.h>
#include <quarantine.h>

void cos_init(void)
{
	spdid_t new_spd;
	printc("UNIT TEST quarantine_fork\n");
	new_spd = quarantine_fork(cos_spd_id(), cos_spd_id());
	printc("UNIT TEST PASSED: quarantine_fork\n");
	return;
}


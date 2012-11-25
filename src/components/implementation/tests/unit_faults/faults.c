/**
 * Copyright 2011 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <cos_component.h>
#include <print.h>

int divisor = 0, done;

void cos_init(void)
{
	printc("Fault unit test commencing.\n");
	done = 10/divisor;
	printc("Fault unit test done.\n");
	return;
}

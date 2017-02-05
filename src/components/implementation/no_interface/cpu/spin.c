/**
 * Copyright 2009 by Gabriel Parmer, gparmer@gwu.edu.  All rights
 * reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <cos_component.h>
#include <print.h>

int spin_var = 0, other;

void cos_init(void *arg)
{
//	BUG();
//	spin_var = *(int*)NULL;

	printc("Core %ld: <<**Running!**>>\n", cos_cpuid());

//	while (1) if (spin_var) other = 1;

	return;
}

/**
 * Copyright 2011 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <cos_component.h>

/* dependencies */
#include <boot_deps.h>
#include <cobj_format.h>


void cos_init(void)
{

	LOCK();
  printc("\nINITIALIZING RUMPKERNEL\n");

	UNLOCK();

	return;
}

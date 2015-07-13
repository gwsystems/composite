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

  printc("\nRUMP KERNEL: START\n");
  //call_rump_init();
  //rump_init();
  printc("\nRUMP KERNEL: DONE\n");

	UNLOCK();

	return;
}

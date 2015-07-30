#include "rumpcalls.h"

#include <stdio.h>
#include <cos_component.h>

extern struct cos_rumpcalls crcalls;

/* Mapping the functions from rumpkernel to composite */

void
cos2rump_setup(void)
{

	crcalls.rump_cos_print = cos_print;
	crcalls.rump_vsnprintf = vsnprintf;

	return;
}

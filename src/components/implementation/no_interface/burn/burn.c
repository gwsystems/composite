#include <llprint.h>

void
cos_init(void)
{
	/* Burn all of the cycles! CPUs are glorified space heaters, right? */
	printc("CPU cycle burn: component %ld, thread id %ld, core %d.\n",
	       cos_compid(), cos_thdid(), cos_coreid());

	while (1) ;
}

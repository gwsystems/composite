#include "cos_init.h"

void
cos_run(char *cmdline)
{
	printc("Rump Kernel bootstrap on platform Composite\n");

	bmk_sched_startmain(bmk_mainthread, cmdline);
}

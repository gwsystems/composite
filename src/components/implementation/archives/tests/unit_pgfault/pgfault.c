#include <cos_component.h>
#include <print.h>

#include <unit_pgfault.h>

int unit_pgfault_page_fault(spdid_t spdid)
{
	int v;
	printc("page fault request from spd %d\n", spdid);
	v = *((int *)0);
	printc("ERROR: reached past page fault\n");
	return v;
}


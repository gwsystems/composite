#include <cos_component.h>
#include <print.h>

#include <sched.h>

#include <unit_pgfault.h>

int foo(void) 
{
	return unit_pgfault_page_fault(cos_spd_id());
}

void cos_init(void)
{
	printc("Starting page fault test...\n");
	foo();
	printc("... and successfully finishing page fault test.\n");
}


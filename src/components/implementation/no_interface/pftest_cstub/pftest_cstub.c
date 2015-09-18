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
	int status;
	printc("Starting page fault test...\n");
	status = foo();
	assert(status == EFAULT);
	printc("... and successfully finishing page fault test.\n");
}


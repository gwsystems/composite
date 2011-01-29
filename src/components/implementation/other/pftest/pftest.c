#include <cos_component.h>
#include <print.h>

#include <pgfault.h>
#include <sched.h>

void cos_init(void)
{
	int v;
	printc("Starting page fault test...\n");
	v = *((int *)0);
	printc("... and successfully finishing page fault test.\n");
}

void bin (void)
{
	fault_page_fault_handler(cos_spd_id(), NULL, 0, NULL);
	sched_block(cos_spd_id(), 0);
}


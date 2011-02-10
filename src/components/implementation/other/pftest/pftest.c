#include <cos_component.h>
#include <print.h>

#include <pgfault.h>
#include <sched.h>

int bar(void)
{
	int v;
	v = *((int *)0);
	return v;
}

int foo(void) 
{
	return bar();
}

void cos_init(void)
{
	printc("Starting page fault test...\n");
	foo();
	printc("... and successfully finishing page fault test.\n");
}

/* void bin (void) */
/* { */
/* 	fault_page_fault_handler(cos_spd_id(), NULL, 0, NULL); */
/* 	sched_block(cos_spd_id(), 0); */
/* } */


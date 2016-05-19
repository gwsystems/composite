#include <cos_component.h>
#include <print.h>
#include <quarantine.h>
#include <test_malloc_comp.h>

#define ITER (1024)
u64_t meas[ITER];

/*
 * Warning - test assumes Composite can run at least ITER components.
 * (And this may not take into account other components for this test)
 */
	
void cos_init(void)
{
	spdid_t f;
	printc("Starting a fork test with dynamic memory allocation\n");

	int comp2fork = 12; 	/* this is the HARDCODED spdid for the component we are going to fork. If you change the runscript, change this too. For this test, this is po.o which is an alias for malloccomp.o */

	call();

	f = quarantine_fork(cos_spd_id(), comp2fork);
	if (f == 0) printc("Error: initial fork failed\n");

	printc("Trying to do call again.\n");		// quarantine determines which spdid this refers to. Apparently the original?
	call();

	return;
}

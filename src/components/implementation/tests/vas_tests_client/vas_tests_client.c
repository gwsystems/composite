#include <cos_kernel_api.h>
#include <cos_types.h>
#include <vas_test_call_a.h>


/*
 * TODO
 */
int
main(void)
{
	printc("Testing chained sinvs...\n");

	printc("Component %ld calling server...\n", cos_compid());
	vas_test_call_a();

	/* if we don't fault or fail an assert, we are good */
	printc("SUCCESS\n");
}

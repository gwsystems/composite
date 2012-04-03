#include <cos_component.h>
#include <print.h>
#include <stdlib.h> 		/* rand */

void cos_init(void)
{
	printc("UNIT TEST Unit tests for initial arguments...\n");
	assert(!strcmp("hello world", cos_init_args()));
	printc("UNIT TEST ALL PASSED\n");

	return;
}

#include <cos_component.h>
#include <print.h>
#include <stdlib.h> 		/* rand */

CCTOR void c1(void) { return; }
CCTOR void c2(void) { return; }

void cos_init(void)
{
	printc("UNIT TEST Unit tests for initial arguments...\n");
	assert(!strcmp("hello world", cos_init_args()));
	printc("UNIT TEST for init args PASSED\n");

	{
		extern long __CTOR_LIST__;
		long *l = &__CTOR_LIST__;
		printc("UNIT TEST ctors\n");
		assert(l[0] == 2);
		assert((long)c1 == l[1]);
		assert((long)c2 == l[2]);
		printc("UNIT TEST ctors PASSED\n");
	}

	return;
}

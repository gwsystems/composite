
#include <cos_component.h>
#include <print.h>

extern void cbuf_tests();
extern void cbufp_tests();

void cos_init(void)
{
	printc("\nUNIT TEST (CBUF & CBUFP)\n");
	cbuf_tests();
	cbufp_tests();
	printc("UNIT TEST (CBUF & CBUFP) ALL PASSED\n");
	return;
}


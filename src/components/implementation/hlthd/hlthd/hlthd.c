#include <cos_component.h>
#include <print.h>
#include <timed_blk.h>
#include <hlc.h>

void call_low()
{
//	int *stkflags;

//	stkflags = ((int*)(round_to_page((int*)&stkflags)));
//	assert(*stkflags == 0);
	printc("In low -- thd: %ld\n", cos_get_thd_id());
	/* when the stack is revoked, return immediately... */
//	while (*stkflags == 0) ;
	timed_event_block(cos_spd_id(), 1);
}

void call_high()
{
	printc("In high -- thd: %ld\n", cos_get_thd_id());
}

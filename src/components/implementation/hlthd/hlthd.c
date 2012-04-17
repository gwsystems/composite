#include <cos_component.h>
#include <print.h>
#include <timed_blk.h>
#include <hlc.h>

void call_low()
{
	printc("In low -- thd: %d\n", cos_get_thd_id());
	/* when the stack is revoked, return immediately... */
//	while (!cos_comp_info.cos_poly[0]) ;
	timed_event_block(cos_spd_id(), 1);
}

void call_high()
{
	printc("In high -- thd: %d\n", cos_get_thd_id());
}

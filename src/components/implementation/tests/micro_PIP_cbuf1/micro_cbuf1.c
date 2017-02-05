#include <cos_component.h>
#include <print.h>
#include <micro_cbuf2.h>
#include <res_spec.h>
#include <sched.h>

void cos_init(void)
{
	static int first = 0;
	union sched_param sp;

	int thd_id;

	if(first == 0){
		printc("<<< CBUF PIP MICRO BENCHMARK TEST  >>>\n");
		first = 1;

		thd_id = cos_get_thd_id();

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 10;
		sched_create_thd(cos_spd_id(), sp.v, 0, 0);
		
		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 11;
		sched_create_thd(cos_spd_id(), sp.v, 0, 0);
	} else {
		call_cbuf2();
	}
	printc("<<< CBUF PIP MICRO BENCHMARK TEST DONE! >>>\n");

	return;
}

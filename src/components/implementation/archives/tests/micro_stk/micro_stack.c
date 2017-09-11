#include <cos_component.h>
#include <print.h>
#include <micro_cbuf2.h>
#include <res_spec.h>
#include <sched.h>

/* MEASURE_CACHED_STK   */
/* Change to thread_pool_1 and use lmicro_stk.sh */

void cos_init(void)
{
	static int first = 0;
	union sched_param sp;
	int thd_id;

	/* printc("<<< STK MICRO BENCHMARK TEST  >>>\n"); */
	if(first == 0){
		first = 1;

		thd_id = cos_get_thd_id();

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 10;
		sched_create_thd(cos_spd_id(), sp.v, 0, 0);
		
		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 11;
		sched_create_thd(cos_spd_id(), sp.v, 0, 0);
	} else {
		call_stk();
	}
	/* printc("<<< STK MICRO BENCHMARK TEST DONE! >>>\n"); */

	return;

}



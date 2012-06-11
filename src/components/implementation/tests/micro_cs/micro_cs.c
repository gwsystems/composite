#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <res_spec.h>
#include <micro_pong.h>

/* 
   changes: max_pool
*/

#define ITER 2000

void cos_init(void)
{
	static int flag = 0;
	union sched_param sp;
	int i;

	if(flag == 0){
		printc("<<< CONTEXT SWITCH MICRO BENCHMARK TEST  >>>\n");
		flag = 1;

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 10;
		sched_create_thd(cos_spd_id(), sp.v, 0, 0);

		sp.c.type = SCHEDP_PRIO;
		sp.c.value = 11;
		sched_create_thd(cos_spd_id(), sp.v, 0, 0);

	} else {
		for(i=0; i<ITER; i++){
			call_cs();
		}
	}

	printc("<<< CONTEXT SWITCH MICRO BENCHMARK TEST DONE >>>\n");

	return;
}


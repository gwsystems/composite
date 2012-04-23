#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <micro_cbuf3.h>

void call_cbuf2(void)
{
	static int first = 0;
	static int low = 0, high = 0;
	
	if (first == 0){
		high = cos_get_thd_id();
		low = high + 1;
		first = 1;
	}
	int j = 0;
	while(j++ < 100){
		if (cos_get_thd_id() == high) {
			sched_block(cos_spd_id(), 0);
		}
		call_cbuf3(low, high);
	}
	return;
}

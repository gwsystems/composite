#include <cos_component.h>
#include <llprint.h>

#include <cos_time.h>
#include <perfdata.h>
#include <syncipc.h>


int
main(void)
{
	thdid_t tid = cos_thdid();
	sched_param_t sp= SCHED_PARAM_CONS(SCHEDP_PRIO, 6);
	word_t ret0 = 0, ret1 = 0;

	sched_thd_param_set(tid, sp);
	while (1) {
		int ret;
		word_t arg0 = ret0, arg1 = ret1;

		ret = syncipc_reply_wait(0, arg0, arg1, &ret0, &ret1);
		if (ret != 0) {
			printc("syncipc benchmark: server reply_wait returned %d\n", ret);
		}
	}
}

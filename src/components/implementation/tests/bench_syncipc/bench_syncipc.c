#include <cos_component.h>
#include <llprint.h>

#include <cos_time.h>
#include <perfdata.h>
#include <syncipc.h>

#define ITERATION 256
struct perfdata perf;
cycles_t results[ITERATION] = {0, };

static void
client(void *d)
{
	word_t arg0 = 0, arg1 = 1;
	cycles_t start, end;
	int i;

	sched_thd_block_timeout(0, time_now() + (1 << 15));

	for (i = 0; i < ITERATION; i++) {
		word_t ret0 = 0, ret1 = 0;
		int ret;

		start = time_now();
		ret   = syncipc_call(0, arg0, arg1, &ret0, &ret1);
		end   = time_now();
		/*
		 * This assertion might mean that the client executed
		 * *before* the server (ret == -EAGAIN), which means
		 * that the delay above is not sufficient.
		 */
		assert(ret == 0);

		arg0++;
		arg1++;

		perfdata_add(&perf, end - start);
	}
	perfdata_calc(&perf);
	perfdata_print(&perf);

	printc("SUCCESS: synchronous IPC between threads\n");

	sched_thd_block(0);
}

static void
server(void *d)
{
	word_t ret0 = 0, ret1 = 0;

	while (1) {
		int ret;
		word_t arg0 = ret0, arg1 = ret1;

		ret = syncipc_reply_wait(0, arg0, arg1, &ret0, &ret1);
		if (ret != 0) {
			printc("syncipc benchmark: server reply_wait returned %d\n", ret);
		}
	}
}

int
main(void)
{
	thdid_t tid;
	sched_param_t sps[] = {
		SCHED_PARAM_CONS(SCHEDP_PRIO, 4),
		SCHED_PARAM_CONS(SCHEDP_PRIO, 6),
	};

	perfdata_init(&perf, "Synchronous IPC round trip latency", results, ITERATION);

	tid = sched_thd_create(client, NULL);
	assert(tid > 0);
	sched_thd_param_set(tid, sps[0]);

	tid = sched_thd_create(server, NULL);
	assert(tid > 0);
	sched_thd_param_set(tid, sps[1]);

	sched_thd_block(0);
}

#include <cos_component.h>
#include <llprint.h>
#include <chan.h>
#include <ps.h>

struct chan_snd s;
struct chan_rcv r;

#define COMM_AMNT (2^10 * 16)

void
receiver(void)
{
	int i;
	ps_tsc_t tot = 0;

	for (i = 0; i < COMM_AMNT; i++) {
		ps_tsc_t now, snd;

		if (chan_recv(&r, &snd, 0)) {
			printc("chan_recv error\n");
			assert(0);
		}
		now = ps_tsc();
		tot += now - snd;
	}

	printc("Average send -> receive overhead %lld\n", tot/COMM_AMNT);
}

void
ipc(void)
{
	int i;
	ps_tsc_t rcv = 0, prev_rcv, snd = 0, tmp;
	ps_tsc_t rcvcost = 0, sendcost = 0, rtt = 0, l2h = 0;

	for (i = 0; i < COMM_AMNT; i++) {
		snd = ps_tsc();
		if (rcv > 0) {
			sendcost += snd - rcv;
		}
		if (chan_recv(&r, &tmp, 0)) {
			printc("chan_recv error\n");
			assert(0);
		}

		prev_rcv = rcv;
		rcv = ps_tsc();
		l2h += rcv - tmp;

		rcvcost += rcv - snd;
		rtt     += rcv - prev_rcv;
		if (chan_send(&s, &rcv, 0)) {
			printc("chan_send error\n");
			assert(0);
		}
	}

	printc("Thread with high priority (4):\n\trcv  %lld\n\tsend %lld\n\trtt  %lld\n\tlow->high %lld\n",
	       rcvcost/COMM_AMNT, sendcost/COMM_AMNT, rtt/COMM_AMNT, l2h/COMM_AMNT);
}

int
main(void)
{
	printc("Component chan hi: executing main.\n");
	receiver();
	ipc();

	return 0;
}

void
cos_init(void)
{
	printc("Component chan hi initializing:\n\tCreate channel 1\n");
	if (chan_snd_init_with(&s, 1, sizeof(u64_t), 128, CHAN_DEFAULT)) {
		printc("Chan test 1 (%ld): Could not initialize send.\n", cos_compid());
		BUG();
	}
	printc("\tCreate channel 2\n");
	if (chan_rcv_init_with(&r, 2, sizeof(u64_t), 128, CHAN_DEFAULT)) {
		printc("Chan test 1 (%ld): Could not initialize recv.\n", cos_compid());
		BUG();
	}

	printc("\tCreate channel 2\n");
	if (sched_thd_param_set(cos_thdid(), sched_param_pack(SCHEDP_PRIO, 4))) {
		printc("sched_thd_param_set failed.\n");
		assert(0);
	}
}

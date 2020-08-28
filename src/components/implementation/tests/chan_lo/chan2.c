#include <cos_component.h>
#include <llprint.h>
#include <chan.h>
#include <ps.h>

struct chan_snd s;
struct chan_rcv r;

#define COMM_AMNT (2^10 * 16)

/*
 * Sync up chan_hi and chan_lo for benchmarks! 
 * Because this is chan_lo, recv first, and when it wakes up from rcv, send which immediately should yield to chan_hi
 */
void
rendezvous(void)
{
	ps_tsc_t rcv = 0, snd = 0;
	if (chan_recv(&r, &rcv, 0)) {
		printc("chan_recv error\n");
		assert(0);
	}

	snd = ps_tsc();
	if (chan_send(&s, &snd, 0)) {
		printc("chan_send error\n");
		assert(0);
	}
}

void
sender(void)
{
	int i;

	rendezvous();
	for (i = 0; i < COMM_AMNT; i++) {
		ps_tsc_t tsc = ps_tsc();

		if (chan_send(&s, &tsc, 0)) {
			printc("chan_send error\n");
			assert(0);
		}
	}
}

void
ipc(void)
{
	int i;
	ps_tsc_t prev_snd = 0, snd = 0, rcv = 0, tmp = 0;
	ps_tsc_t rcvcost = 0, sendcost = 0, rtt = 0, h2l = 0;

	for (i = 0; i < COMM_AMNT; i++) {
		prev_snd = snd;
		snd = ps_tsc();
		if (prev_snd != 0) {
			rtt     += snd - prev_snd;
			rcvcost += snd - rcv;
		}
		if (tmp != 0) {
			h2l += snd - tmp;
		}
		if (chan_send(&s, &snd, 0)) {
			printc("chan_send error\n");
			assert(0);
		}

		rcv = ps_tsc();
		sendcost += rcv - snd;
		if (chan_recv(&r, &tmp, 0)) {
			printc("chan_recv error\n");
			assert(0);
		}
	}
	/* Sync up with chan_hi here..*/

	printc("Thread with low priority (5):\n\trcv  %lld\n\tsend %lld\n\trtt  %lld\n\thigh->low %lld\n",
	       rcvcost/COMM_AMNT, sendcost/COMM_AMNT, rtt/COMM_AMNT, h2l/COMM_AMNT);
}

int
main(void)
{
	printc("Component chan lo: executing main.\n");
	sender();
	ipc();

	return 0;
}

void
cos_init(void)
{
	memset(&s, 0, sizeof(struct chan_snd));
	memset(&r, 0, sizeof(struct chan_rcv));
	printc("Component chan lo initializing:\n\tJoin channel 2\n");
	if (chan_snd_init_with(&s, 2, sizeof(u64_t), 128, CHAN_DEFAULT)) {
		printc("Chan test 2 (%ld): Could not initialize send.\n", cos_compid());
		BUG();
	}
	printc("\tJoin channel 1\n");
	if (chan_rcv_init_with(&r, 1, sizeof(u64_t), 128, CHAN_DEFAULT)) {
		printc("Chan test 2 (%ld): Could not initialize recv.\n", cos_compid());
		BUG();
	}

	printc("\tPriority 5 for self!\n");
	if (sched_thd_param_set(cos_thdid(), sched_param_pack(SCHEDP_PRIO, 5))) {
		printc("sched_thd_param_set failed.\n");
		BUG();
	}
}

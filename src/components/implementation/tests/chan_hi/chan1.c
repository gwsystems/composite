#include <cos_component.h>
#include <llprint.h>
#include <chan.h>
#include <ps.h>
#include <cos_time.h>

struct chan_snd s;
struct chan_rcv r;

#define COMM_AMNT (2^10 * 16)

/*
 * the Receiver and sender tests could run at different times,
 * causing the numbers to blow up!
 *
 * Another issue is, when the high priority thread here is printing,
 * the test on the low prio side is not done yet!
 *
 * So, this routine syncs up between chan_hi and chan_lo wherever required!
 * Because this (chan_hi) is high priority thread, send first and wait on rcv!
 * The chan_lo would receive first and then send, that should immediately switch over to here!
 */
void
rendezvous(void)
{
	ps_tsc_t rcv = 0, snd = 0;
	snd = ps_tsc();
	if (chan_send(&s, &snd, 0)) {
		printc("chan_send error\n");
		assert(0);
	}

	if (chan_recv(&r, &rcv, 0)) {
		printc("chan_recv error\n");
		assert(0);
	}
}

void
receiver(void)
{
	int i;
	ps_tsc_t tot = 0;

	rendezvous();
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
	ps_tsc_t rcv = 0, prev_rcv = 0, snd = 0, tmp;
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
		if (prev_rcv != 0) rtt += rcv - prev_rcv;
		if (chan_send(&s, &rcv, 0)) {
			printc("chan_send error\n");
			assert(0);
		}
	}
	/* Sync up here before printing. Chan_lo is *very likely* not done with its measurements yet! */
	rendezvous();

	printc("Thread with high priority (4):\n\trcv  %lld\n\tsend %lld\n\trtt  %lld\n\tlow->high %lld\n",
	       rcvcost/COMM_AMNT, sendcost/COMM_AMNT, rtt/COMM_AMNT, l2h/COMM_AMNT);
}

int
main(void)
{
	cycles_t wakeup;

	printc("Component chan hi: executing main.\n");

	/*
	 * This sleep in both hi and lo comps lets the benchmark run
	 * more predictably on HW and on Qemu.
	 *
	 * Likely because this helps the priority change in cos_init take effect!
	 * Or because this lets the initialization of both ends of channels complete before tests start!
	 */
	wakeup = time_now() + time_usec2cyc(10 * 1000);
	sched_thd_block_timeout(0, wakeup);

	receiver();
	ipc();

	return 0;
}

#define TEST_CHAN_ITEM_SZ   sizeof(u64_t)
#define TEST_CHAN_NSLOTS    128
#define TEST_CHAN_SEND_ID   1
#define TEST_CHAN_RECV_ID   2
#define TEST_CHAN_PRIO_SELF 4 /* chan_hi == high priority */

void
cos_init(void)
{
	memset(&s, 0, sizeof(struct chan_snd));
	memset(&r, 0, sizeof(struct chan_rcv));
	printc("Component chan hi initializing:\n\tCreate channel %d\n", TEST_CHAN_SEND_ID);
	if (chan_snd_init_with(&s, TEST_CHAN_SEND_ID, TEST_CHAN_ITEM_SZ, TEST_CHAN_NSLOTS, CHAN_DEFAULT)) {
		printc("Chan test 1 (%ld): Could not initialize send.\n", cos_compid());
		BUG();
	}
	printc("\tCreate channel %d\n", TEST_CHAN_RECV_ID);
	if (chan_rcv_init_with(&r, TEST_CHAN_RECV_ID, TEST_CHAN_ITEM_SZ, TEST_CHAN_NSLOTS, CHAN_DEFAULT)) {
		printc("Chan test 1 (%ld): Could not initialize recv.\n", cos_compid());
		BUG();
	}

	printc("\tPriority %d for self!\n", TEST_CHAN_PRIO_SELF);
	if (sched_thd_param_set(cos_thdid(), sched_param_pack(SCHEDP_PRIO, TEST_CHAN_PRIO_SELF))) {
		printc("sched_thd_param_set failed.\n");
		assert(0);
	}
}

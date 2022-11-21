#include <cos_component.h>
#include <llprint.h>
#include <chan.h>
#include <ps.h>
#include <perfdata.h>
#include <cos_time.h>

#undef CHAN_TRACE_DEBUG
#ifdef CHAN_TRACE_DEBUG
#define debug(format, ...) printc(format, ##__VA_ARGS__)
#else
#define debug(format, ...)
#endif

struct chan_snd s;
struct chan_rcv r;
struct evt e;

/* Keep these settings below consistent with the sender side */
#define READER_HIGH
#define USE_EVTMGR

#define TEST_CHAN_ITEM_SZ   sizeof(u32_t)
#define TEST_CHAN_NSLOTS    2
#define TEST_CHAN_SEND_ID   3
#define TEST_CHAN_RECV_ID   4
/* We are the receiver, and we don't care about data gathering */
#ifdef READER_HIGH
#define TEST_CHAN_PRIO_SELF 4
#else
#define TEST_CHAN_PRIO_SELF 5
#endif

typedef unsigned int cycles_32_t;

int
main(void)
{
	cycles_t wakeup;
	cycles_32_t tmp;
#ifdef USE_EVTMGR
	evt_res_id_t evt_id;
	evt_res_data_t evtdata;
	evt_res_type_t  evtsrc;
#endif

	printc("Component chan receiver: executing main.\n");

	/* See if event manager is in use. If yes, log the receiver channel into it */
#ifdef USE_EVTMGR
	assert(evt_init(&e, 2) == 0);
	evt_id = evt_add(&e, 0, (evt_res_data_t)&r);
	assert(evt_id != 0);
	assert(chan_rcv_evt_associate(&r, evt_id) == 0);
	printc("Receiver side event created.\n");
#endif

	/*
	 * This sleep in both hi and lo comps lets the benchmark run
	 * more predictably on HW and on Qemu.
	 *
	 * Likely because this helps the priority change in cos_init take effect!
	 * Or because this lets the initialization of both ends of channels complete before tests start!
	 */
	wakeup = time_now() + time_usec2cyc(100 * 1000);
	sched_thd_block_timeout(0, wakeup);

	/* Never stops running; sender controls how many iters to run. */
	while(1) {
		debug("r1,");
#ifdef USE_EVTMGR
		/* Receive from the events then the channel */
		while (chan_recv(&r, &tmp, CHAN_NONBLOCKING) == CHAN_TRY_AGAIN) evt_get(&e, EVT_WAIT_DEFAULT, &evtsrc, &evtdata);
#else
		chan_recv(&r, &tmp, 0);
#endif
		debug("tsr1: %d,", tmp);
		debug("r2,");
		tmp = time_now();
		debug("tsr2: %d,", tmp);
		debug("r3,");
		chan_send(&s, &tmp, 0);
		debug("r4,");
	}
}


/* We initialize channel and threads before executing main - here the scheduler doesn't even work so we guarantee good init */
void
cos_init(void)
{
	memset(&s, 0, sizeof(struct chan_snd));
	memset(&r, 0, sizeof(struct chan_rcv));
	printc("Component chan receiver initializing:\n\tCreate channel %d\n", TEST_CHAN_SEND_ID);
	if (chan_snd_init_with(&s, TEST_CHAN_SEND_ID, TEST_CHAN_ITEM_SZ, TEST_CHAN_NSLOTS, CHAN_DEFAULT)) {
		printc("Chan test 1 (%ld): Could not initialize send.\n", cos_compid());
		BUG();
	}
	printc("\tCreate channel %d\n", TEST_CHAN_RECV_ID);
	if (chan_rcv_init_with(&r, TEST_CHAN_RECV_ID, TEST_CHAN_ITEM_SZ, TEST_CHAN_NSLOTS, CHAN_DEFAULT)) {
		printc("Chan test 1 (%ld): Could not initialize recv.\n", cos_compid());
		BUG();
	}

	printc("Recv: smem %p, rmem %p.\n", s.meta.mem, r.meta.mem);

	printc("\tPriority %d for self!\n", TEST_CHAN_PRIO_SELF);
	if (sched_thd_param_set(cos_thdid(), sched_param_pack(SCHEDP_PRIO, TEST_CHAN_PRIO_SELF))) {
		printc("sched_thd_param_set failed.\n");
		assert(0);
	}
}

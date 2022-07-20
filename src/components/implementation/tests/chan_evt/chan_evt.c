#include <cos_component.h>
#include <llprint.h>
#include <chan.h>
#include <ps.h>
#include <cos_time.h>
#include <sched.h>

/***
 * There will be three threads. Only one is a receiver, and two will be senders. The receiver have a
 * higher priority, and all senders have a low priority. The senders will compete and send data two
 * their corresponding channels a fixed number. The receiver receives on the event and detects whatever
 * the data is.
 */

#define COMM_AMNT       (10000)
#define SEND_THD_NUM    (4)


struct chan_test
{
	int idx;
	thdid_t s_id;
	evt_res_id_t evt_id;
	struct chan c;
	struct chan_snd s;
	struct chan_rcv r;
};

struct chan_test test[SEND_THD_NUM];
thdid_t init_thd;
thdid_t r_id;
struct evt e;

void
sender(void *d)
{
	int i;
	word_t idx;

	idx = (word_t)d;

	for (i = 0; i < COMM_AMNT; i++) {
		thdid_t tid = cos_thdid();
		assert(tid == test[idx].s_id);

		/* Send to our TID to our own respective channels */
		if (chan_send(&(test[idx].s), &tid, 0)) {
			printc("chan_send error\n");
			assert(0);
		}
	}

	printc("Sender %lu finished\n", idx);
	sched_thd_block(0);
	assert(0);
}

void
receiver(void *d)
{
	int i;

	for (i = 0; i < COMM_AMNT*SEND_THD_NUM; i++) {
		thdid_t snd;
		evt_res_data_t evtdata;
		evt_res_type_t  evtsrc;
		struct chan_rcv *r;

		/* Receive from the events */
		if (evt_get(&e, EVT_WAIT_DEFAULT, &evtsrc, &evtdata))  {
			printc("evt_get error\n");
			assert(0);
		}

		/* Then the channel */
		r = (struct chan_rcv*)evtdata;
		if (chan_recv(r, &snd, CHAN_NONBLOCKING)) {
			printc("chan_recv error\n");
			assert(0);
		}

		/* Must be equal or we messed up something */
		if (test[evtsrc].s_id != snd)  {
			printc("chan_recv value error - recv %lu, should be %lu\n", snd, test[evtsrc].s_id);
			assert(0);
		}
	}

	printc("Receiver finished\n");

	sched_thd_wakeup(init_thd);
	sched_thd_block(0);
	assert(0);
}

int
main(void)
{
	/* Create channels */
	for (int i = 0; i < SEND_THD_NUM; i++) {
		test[i].idx = i;

		if (chan_init(&(test[i].c), sizeof(thdid_t), 16, CHAN_DEFAULT)) {
			printc("chan_init failure %d.\n", i);
			assert(0);
		}

		if (chan_snd_init(&(test[i].s), (struct chan *)&(test[i].c))) {
			printc("chan_snd_init failure %d.\n", i);
			assert(0);
		}

		if (chan_rcv_init(&(test[i].r), (struct chan *)&(test[i].c))) {
			printc("chan_rcv_init failure %d.\n", i);
			assert(0);
		}
	}

	printc("Chan tests: created channel, send/receive end-points. Proceeding with child thread creation.\n");

	/* Create threads */
	init_thd = cos_thdid();
	r_id = sched_thd_create(receiver, NULL);
	if (r_id == 0) {
		printc("sched_thd_create error.\n");
		assert(0);
	}

	for (word_t i = 0; i < SEND_THD_NUM; i++) {
		thdid_t s_id = sched_thd_create(sender, (void*)i);

		if (s_id == 0) {
			printc("sched_thd_create error.\n");
			assert(0);
		}

		test[i].s_id = s_id;

		if (sched_thd_param_set(s_id, sched_param_pack(SCHEDP_PRIO, 5))) {
			printc("sched_thd_param_set failed.\n");
			assert(0);
		}
	}

	if (sched_thd_param_set(r_id, sched_param_pack(SCHEDP_PRIO, 4))) {
		printc("sched_thd_param_set failed.\n");
		assert(0);
	}

	/* Bind all to an event */
	printc("Chan tests: created child threads. Proceeding with event creation.\n");
	assert(evt_init(&e, SEND_THD_NUM) == 0);

	for (int i = 0; i < SEND_THD_NUM; i++) {
		test[i].evt_id = evt_add(&e, i, (evt_res_data_t)&(test[i].r));
		assert(test[i].evt_id != 0);
		assert(chan_evt_associate(&(test[i].c), test[i].evt_id) == 0);
	}

	printc("Chan tests: created child threads. Proceeding with tests.\n");
	sched_thd_block(0);
	printc("Chan evt test: SUCCESS.\n");

	return 0;
}

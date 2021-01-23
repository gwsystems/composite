#include <cos_component.h>
#include <llprint.h>
#include <chan.h>
#include <ps.h>

struct chan_snd s;
struct chan_rcv r;
thdid_t         init_thd;

#define COMM_AMNT (2 ^ 10 * 16)

void
sender(void *d)
{
	int i;

	for (i = 0; i < COMM_AMNT; i++) {
		ps_tsc_t tsc = ps_tsc();

		if (chan_send(&s, &tsc, 0)) {
			printc("chan_send error\n");
			assert(0);
		}
	}
	sched_thd_block(0);
	assert(0);
}

void
receiver(void *d)
{
	int      i;
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

	printc("Average send -> receive overhead %lld\n", tot / COMM_AMNT);

	sched_thd_wakeup(init_thd);
	sched_thd_block(0);
	assert(0);
}

int
main(void)
{
	struct chan c;
	cos_thdid_t s_id, r_id;

	if (chan_init(&c, sizeof(ps_tsc_t), 128, CHAN_DEFAULT)) {
		printc("chan_init failure.\n");
		assert(0);
	}

	if (chan_snd_init(&s, &c)) {
		printc("chan_snd_init failure.\n");
		assert(0);
	}

	if (chan_rcv_init(&r, &c)) {
		printc("chan_rcv_init failure.\n");
		assert(0);
	}

	printc("Chan tests: created channel, send/receive end-points. Proceeding with child thread creation.\n");

	init_thd = cos_thdid();
	s_id     = sched_thd_create(sender, NULL);
	r_id     = sched_thd_create(receiver, NULL);
	if (s_id == 0 || r_id == 0) {
		printc("sched_thd_create error.\n");
		assert(0);
	}

	if (sched_thd_param_set(s_id, sched_param_pack(SCHEDP_PRIO, 5))) {
		printc("sched_thd_param_set failed.\n");
		assert(0);
	}
	if (sched_thd_param_set(r_id, sched_param_pack(SCHEDP_PRIO, 4))) {
		printc("sched_thd_param_set failed.\n");
		assert(0);
	}

	sched_thd_block(0);
	printc("Chan test: SUCCESS.\n");

	return 0;
}

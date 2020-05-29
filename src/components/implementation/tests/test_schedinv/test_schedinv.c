/*
 * Copyright 2018, Phani Gadepalli and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <llprint.h>
#include <res_spec.h>
#include <hypercall.h>
#include <sched.h>
#include <capmgr.h>
#include <chan_crt.h>
#include <channel.h>
#include <cos_time.h>

#define SPDID_INT 5
#define SPDID_W1  6
#define SPDID_W3  7

static u32_t cycs_per_usec = 0;

#define MAX_USE_PIPE_SZ 1

#define SND_DATA 0x4321
#define HPET_PERIOD_TEST_US 20000

#define SHMCHANNEL_KEY 0x2020
static cycles_t *sttsc = NULL;
volatile unsigned long *rdy = NULL;
int iters = 0;
#define ITERS 100000
cycles_t vals[ITERS] = { 0 };

static void
__test_int_fn(arcvcap_t rcv, void *data)
{
	ps_faa(rdy, 1);

	while (ps_load(rdy) <= MAX_USE_PIPE_SZ) sched_thd_block_timeout(0, time_now() + time_usec2cyc(HPET_PERIOD_TEST_US));
	int a = capmgr_hw_periodic_attach(HW_HPET_PERIODIC, cos_thdid(), HPET_PERIOD_TEST_US);
	assert(a == 0);

	/* TODO: register to HPET */
	while (1) {
		cos_rcv(rcv, 0);
		iters++;
		rdtscll(*sttsc);
		chan_out(SND_DATA);

		if (iters == ITERS) capmgr_hw_detach(HW_HPET_PERIODIC);
	}

	sched_thd_exit();
}

cycles_t tot = 0, wc = 0;

static void
__test_wrk_fn(void *data)
{
	int e = (int) data;
	ps_faa(rdy, 1);
	while (1) {
		chan_in();

		if (unlikely(e)) {
			cycles_t en, diff;

			if (unlikely(iters >= ITERS)) continue;
			rdtscll(en);
			assert(sttsc);
			diff = en - *sttsc;
			if (diff > wc) wc = diff;
			tot += diff;
			vals[iters] = diff;
			//printc("%llu\n", diff);
			iters++;
			if (iters % 1000 == 0) printc(",");
			if (iters == ITERS) {
				int i;

				for (i = 0; i < ITERS; i++) printc("%llu\n", vals[i]);
				PRINTC("%llu, %llu\n", tot / ITERS, wc);
				tot = wc = 0;
				//iters = 0;
			}
			continue;
		}
		chan_out(SND_DATA);
	}
}

struct cos_aep_info intaep;

static void
test_aeps(void)
{
	thdid_t tid;
	int ret;
	int i = 0;

	if (cos_spd_id() == SPDID_INT) {
		tid = sched_aep_create(&intaep, __test_int_fn, (void *)0, 0, 0, 0, 0);
	} else {
		tid = sched_thd_create(__test_wrk_fn, 
			((cos_spd_id() == SPDID_W3 && MAX_USE_PIPE_SZ == 4) 
			|| (cos_spd_id() == SPDID_W1 && MAX_USE_PIPE_SZ == 2)) 
			? (void *)1: (void *)0);
	}
	assert(tid);
}

void
cos_init(void)
{
	spdid_t child;
	comp_flag_t childflags;

	vaddr_t addr = 0;
	unsigned long pages = 0;
	cbuf_t id =  channel_shared_page_map(SHMCHANNEL_KEY, &addr, &pages);
	assert(id > 0 && addr && pages == 1);
	sttsc = (cycles_t *)addr;
	rdy = (volatile unsigned long *)(sttsc + 1);

	cycs_per_usec = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);

	assert(hypercall_comp_child_next(cos_spd_id(), &child, &childflags) == -1);
	test_aeps();
	PRINTC("Init Done!\n");

	sched_thd_exit();
}

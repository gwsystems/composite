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

#define SPDID_INT 1
#define SPDID_W1  3
#define SPDID_W3  5

static u32_t cycs_per_usec = 0;

#define SND_DATA 0x4321
#define HPET_PERIOD_TEST_US 5000

#define SHMCHANNEL_KEY 0x2020
static cycles_t *sttsc = NULL;
static void
__test_int_fn(arcvcap_t rcv, void *data)
{
	int a = capmgr_hw_periodic_attach(HW_HPET_PERIODIC, cos_thdid(), HPET_PERIOD_TEST_US);
	assert(a == 0);

	/* TODO: register to HPET */
	while (1) {
		cos_rcv(rcv, 0);
		rdtscll(*sttsc);
		//printc("[i%u]", cos_thdid());
		chan_out(SND_DATA);
	}

	sched_thd_exit();
}

#define ITERS 100
cycles_t tot = 0, wc = 0;
int iters = 0;

static void
__test_wrk_fn(void *data)
{
	int e = (int) data;
	while (1) {
		chan_in();

		if (unlikely(e)) {
			//printc("[e%u]", cos_thdid());
			cycles_t en, diff;

			rdtscll(en);
			assert(sttsc);
			diff = en - *sttsc;
			if (diff > wc) wc = diff;
			tot += diff;
			iters++;
			if (iters == ITERS) {
				printc("%llu, %llu\n", tot / ITERS, wc);	
				tot = wc = 0;
				iters = 0;
			}
			continue;
		} else {
			//printc("[w%u]", cos_thdid());
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
		tid = sched_thd_create(__test_wrk_fn, cos_spd_id() == SPDID_W3 ? (void *)1: (void *)0);
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
	assert(id >= 0 && addr && pages == 1);
	sttsc = (cycles_t *)addr;
	cycs_per_usec = cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);

	assert(hypercall_comp_child_next(cos_spd_id(), &child, &childflags) == -1);
	test_aeps();

	sched_thd_exit();
}

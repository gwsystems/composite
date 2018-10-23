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
#include <channel.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <cos_time.h>
#include "micro_async.h"

#define CLIENT_READY(addr) ((unsigned long *)addr)
#define SERVER_READY(addr) ((unsigned long *)(addr+1))

#define IPC_TSC_ADDR(addr) ((cycles_t *)(addr + 2))

static volatile asndcap_t snd = 0;

static thdid_t thdid;
static struct cos_aep_info thdaep;
static volatile vaddr_t shaddr = 0;
static cbuf_t shid = 0;

static void
test_rcvasnd(arcvcap_t r, void *d)
{
	asndcap_t s = snd;
#ifdef IPC_UBENCH
	cycles_t *mst = IPC_TSC_ADDR(shaddr);
#endif

	assert(s);

	while (1) {
		int ret = 0, rcvd = 0;

		ret = cos_rcv(r, RCV_ALL_PENDING, &rcvd);
		assert(ret == 0 && rcvd == 1);

#ifdef IPC_UBENCH
		assert(*mst == 0);
		rdtscll(*mst);
#endif
		ret = cos_asnd(s, 1);
		assert(ret == 0);
	}

	sched_thd_exit();
}

void
cos_init(void)
{
	int ret;

	if (cos_cpuid() != SERVER_CORE) goto done;

	PRINTC("Started PONG\n");

	while (1) {
		unsigned long npages = 0;

		shid = channel_shared_page_map(SHMEM_KEY, &shaddr, &npages);
		if (shid) {
			assert(shaddr && npages == 1);
			break;
		}
	}
	assert(shid && shaddr);

	thdid = sched_aep_create(&thdaep, test_rcvasnd, NULL, 0, SERVER_KEY, 0, 0);
	assert(thdid);

	while (1) {
#ifdef IPC_RAW
		snd = capmgr_asnd_key_create_raw(CLIENT_KEY);
#else
		snd = capmgr_asnd_key_create(CLIENT_KEY);
#endif
		if (snd) break;
		sched_thd_block_timeout(0, time_now() + time_usec2cyc(TEST_SLEEP));
	}
	assert(snd);

	ret = sched_thd_param_set(thdid, sched_param_pack(SCHEDP_WINDOW, TEST_WINDOW));
	assert(ret == 0);
	ret = sched_thd_param_set(thdid, sched_param_pack(SCHEDP_BUDGET, TEST_BUDGET));
	assert(ret == 0);
	ret = sched_thd_param_set(thdid, sched_param_pack(SCHEDP_PRIO, TEST_PRIO));
	assert(ret == 0);

	ps_faa(SERVER_READY(shaddr), 1);
	while (ps_load(CLIENT_READY(shaddr)) == 0) ;

done:
	sched_thd_exit();
}

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

static volatile asndcap_t snd = 0;

static cycles_t test_cycs[TEST_ITERS] = { 0 };
static unsigned int test_iters = 0;
static thdid_t thdid;
static struct cos_aep_info thdaep;
static volatile vaddr_t shaddr = 0;
static cbuf_t shid = 0;

static void
test_asndrcv(arcvcap_t r, void *d)
{
	asndcap_t s = snd;
	int i;
	int first = 1;
#ifdef IPC_UBENCH
	cycles_t *mst = IPC_TSC_ADDR(shaddr);
#endif

	assert(s);

	while (1) {
		int ret = 0, rcvd = 0;
		cycles_t st, en;

#ifdef IPC_UBENCH
		*mst = 0;
#endif
		rdtscll(st);
		ret = cos_asnd(s, 1);
		assert(ret == 0);

		ret = cos_rcv(r, RCV_ALL_PENDING, &rcvd);
		assert(ret == 0 && rcvd == 1);
		rdtscll(en);

		if (unlikely(first)) {
			first = 0;
			continue;
		}

#ifdef IPC_UBENCH
		assert(*mst && *mst < en);
		test_cycs[test_iters] = en - *mst;
#else
		test_cycs[test_iters] = en - st;
#endif
		test_iters++;

		if (unlikely(test_iters == TEST_ITERS)) break;
	}

	for (i = 0; i < TEST_ITERS; i++) printc("%llu\n", test_cycs[i]);
	printc("---------------------\n");

	sched_thd_exit();
}

void
cos_init(void)
{
	int ret;

	if (cos_cpuid() != CLIENT_CORE) goto done;

	PRINTC("Started PING\n");
	shid = channel_shared_page_alloc(SHMEM_KEY, &shaddr);
	assert(shid && shaddr);

	thdid = sched_aep_create(&thdaep, test_asndrcv, NULL, 0, CLIENT_KEY, 0, 0);
	assert(thdid);
	while (1) {
#ifdef IPC_RAW
		snd = capmgr_asnd_key_create_raw(SERVER_KEY);
#else
		snd = capmgr_asnd_key_create(SERVER_KEY);
#endif
		if (snd) break;
		sched_thd_block_timeout(0, time_now() + time_usec2cyc(TEST_SLEEP));
	}
	assert(snd);

	ps_faa(CLIENT_READY(shaddr), 1);
	while (ps_load(SERVER_READY(shaddr)) == 0) ;

	ret = sched_thd_param_set(thdid, sched_param_pack(SCHEDP_WINDOW, TEST_WINDOW));
	assert(ret == 0);
	ret = sched_thd_param_set(thdid, sched_param_pack(SCHEDP_BUDGET, TEST_BUDGET));
	assert(ret == 0);
	ret = sched_thd_param_set(thdid, sched_param_pack(SCHEDP_PRIO, TEST_PRIO));
	assert(ret == 0);

done:
	sched_thd_exit();
}

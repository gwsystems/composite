/*
 * Copyright 2018, Phani Gadepalli and Gabriel Parmer, GWU, gparmer@gwu.edu.
 *
 * This uses a two clause BSD License.
 */

#include <cos_types.h>
#include <cos_defkernel_api.h>
#include <llprint.h>
#include <res_spec.h>
#include <sl.h>
#include <hypercall.h>
#include <capmgr.h>
#include "perfdata.h"
#include <simple_sl.h>
#include "test_vm.h"
#include <chal_config.h>

#define HI_PRIO TCAP_PRIO_MAX
#define LOW_PRIO (HI_PRIO + 1)

#define AEP_BUDGET_US 10000
#define AEP_PERIOD_US 10000

#define SCHED_PERIOD_US 100000 /* 100ms */

volatile cycles_t c0_start = 0, c0_end = 0, c0_mid = 0, c1_start = 0, c1_end = 0, c1_mid = 0;

static volatile struct perfdata pd;

#define CLIENT_ID 0
#define SERVER_ID 1
#define CLIENT_PRIO HI_PRIO
#define SERVER_PRIO LOW_PRIO
#define MAX_IDS 2

static volatile asndcap_t sndcaps[MAX_IDS] = { 0 };
static volatile arcvcap_t rcvcaps[MAX_IDS] = { 0 };

static void
client_fn(arcvcap_t r, void *d)
{
	asndcap_t snd = sndcaps[SERVER_ID];
	int iters;

#ifdef VM_IPC_TEST
#ifdef TEST_IPC_RAW
	while (!snd) snd = capmgr_asnd_key_create_raw(SERVER_XXX_AEPKEY);
#else
	while (!snd) snd = capmgr_asnd_key_create(SERVER_XXX_AEPKEY);
#endif
#endif

	assert(snd);
	rdtscll(c0_start);
	perfdata_init((struct perfdata *)&pd, "RPC(0<->1)");
	c0_end = c0_mid = c1_start = c1_mid = c1_end = c0_start;

	while (1) {
		int pending = 0, rcvd = 0, ret = 0;
		cycles_t rtt_diff, one_diff = 0;

		rdtscll(c0_start);
		ret = cos_asnd(snd, 0);
		assert(ret == 0);

		pending = cos_rcv(r, RCV_ALL_PENDING, &rcvd);
		assert(pending == 0 && rcvd == 1);
		rdtscll(c0_end);

		rtt_diff = (c0_end - c0_start);
		perfdata_add(&pd, rtt_diff);

		iters++;
		if (iters >= TEST_VM_ITERS)
		{
			break;
		}
	}

	//print stats..
	perfdata_calc(&pd);
	perfdata_print(&pd);

	sl_thd_exit();
}

static void
server_fn(arcvcap_t r, void *d)
{
	asndcap_t snd = sndcaps[CLIENT_ID];

	assert(snd);

	while (1) {
		int pending = 0, rcvd = 0, ret = 0;

		pending = cos_rcv(r, RCV_ALL_PENDING, &rcvd);
		assert(pending == 0 && rcvd >= 1);

		ret = cos_asnd(snd, 0);
		assert(ret == 0);
	}

	sl_thd_exit();
}

static void
test_ipc_setup(void)
{
	static volatile int cdone[NUM_CPU] = { 0 };
	int i, ret;
	asndcap_t snd = 0;

	PRINTC("Setting up end-points\n");

	if (cos_cpuid() == CLIENT_CORE) {
		struct sl_thd *t = NULL;

#ifndef VM_IPC_TEST
		t = sl_thd_aep_alloc(client_fn, (void *)cos_cpuid(), 1, 0, 0, 0);
#else
		t = sl_thd_aep_alloc(client_fn, (void *)cos_cpuid(), 1, CLIENT_XXX_AEPKEY, 0, 0);
#endif
		assert(t);
		rcvcaps[CLIENT_ID] = sl_thd_rcvcap(t);

		ret = cos_tcap_transfer(sl_thd_rcvcap(t), BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, CLIENT_PRIO);
		assert(ret == 0);
		sl_thd_param_set(t, sched_param_pack(SCHEDP_WINDOW, AEP_PERIOD_US));
		sl_thd_param_set(t, sched_param_pack(SCHEDP_BUDGET, AEP_BUDGET_US));
		sl_thd_param_set(t, sched_param_pack(SCHEDP_PRIO, CLIENT_PRIO));
	}

#ifndef VM_IPC_TEST
	if (cos_cpuid() == SERVER_CORE) {
		struct sl_thd *t = NULL;

		t = sl_thd_aep_alloc(server_fn, (void *)cos_cpuid(), 1, 0, 0, 0);
		assert(t);
		rcvcaps[SERVER_ID] = sl_thd_rcvcap(t);

		ret = cos_tcap_transfer(sl_thd_rcvcap(t), BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, SERVER_PRIO);
		assert(ret == 0);
		sl_thd_param_set(t, sched_param_pack(SCHEDP_WINDOW, AEP_PERIOD_US));
		sl_thd_param_set(t, sched_param_pack(SCHEDP_BUDGET, AEP_BUDGET_US));
		sl_thd_param_set(t, sched_param_pack(SCHEDP_PRIO, SERVER_PRIO));
	}
#endif

	if (cos_cpuid() == CLIENT_CORE) {
#ifndef VM_IPC_TEST
		while (!rcvcaps[SERVER_ID]) ;

#ifdef TEST_IPC_RAW
		snd = capmgr_asnd_rcv_create_raw(rcvcaps[SERVER_ID]);
#else
		snd = capmgr_asnd_rcv_create(rcvcaps[SERVER_ID]);
#endif
		assert(snd);
		sndcaps[SERVER_ID] = snd;
#endif
	}
	if (cos_cpuid() == SERVER_CORE) {
		while (!rcvcaps[CLIENT_ID]) ;

#ifdef TEST_IPC_RAW
		snd = capmgr_asnd_rcv_create_raw(rcvcaps[CLIENT_ID]);
#else
		snd = capmgr_asnd_rcv_create(rcvcaps[CLIENT_ID]);
#endif
		assert(snd);
		sndcaps[CLIENT_ID] = snd;
	}

	ps_faa((unsigned long *)&cdone[cos_cpuid()], 1);
	for (i = 0; i < NUM_CPU; i++) {
		while (!ps_load((unsigned long *)&cdone[i])) ;
	}

	PRINTC("Done\n");
}

static volatile cycles_t timenw = 0;

void
loprio_fn(arcvcap_t r, void *d)
{
	cycles_t *nw = (cycles_t *)d;

	while (1) rdtscll(*nw);
}

void
hiprio_fn(arcvcap_t r, void *d)
{
	int iters = 0;
	cycles_t *nw = (cycles_t *)d;

	rdtscll(*nw);

	while (1) {
		cycles_t now = 0;
		int pending = cos_rcv(r, 0, NULL);
		assert(pending == 0);

		iters++;
		rdtscll(now);
		perfdata_add(&pd, now - *nw);
		*nw = now;
		if (iters >= TEST_VM_ITERS) break;
	}

	capmgr_hw_detach(HW_HPET_PERIODIC);
	perfdata_calc(&pd);
	perfdata_print(&pd);
	sl_thd_exit();
}

static void
test_int_setup(void)
{
	static volatile int cdone[NUM_CPU] = { 0 };
	int i, ret;
	asndcap_t snd = 0;
	struct sl_thd *ht = NULL, *lt = NULL;

	PRINTC("Setting up end-points\n");
	assert(NUM_CPU == 1);

	ht = sl_thd_aep_alloc(hiprio_fn, (void *)&timenw, 1, 0, 0, 0);
	assert(ht);

	ret = cos_tcap_transfer(sl_thd_rcvcap(ht), BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, HI_PRIO);
	assert(ret == 0);
	sl_thd_param_set(ht, sched_param_pack(SCHEDP_WINDOW, AEP_PERIOD_US));
	sl_thd_param_set(ht, sched_param_pack(SCHEDP_BUDGET, AEP_BUDGET_US));
	sl_thd_param_set(ht, sched_param_pack(SCHEDP_PRIO, HI_PRIO));

	lt = sl_thd_aep_alloc(loprio_fn, (void *)&timenw, 1, 0, 0, 0);
	assert(lt);

	ret = cos_tcap_transfer(sl_thd_rcvcap(lt), BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, LOW_PRIO);
	assert(ret == 0);
	sl_thd_param_set(lt, sched_param_pack(SCHEDP_WINDOW, AEP_PERIOD_US));
	sl_thd_param_set(lt, sched_param_pack(SCHEDP_BUDGET, AEP_BUDGET_US));
	sl_thd_param_set(lt, sched_param_pack(SCHEDP_PRIO, LOW_PRIO));

	ps_faa((unsigned long *)&cdone[cos_cpuid()], 1);
	for (i = 0; i < NUM_CPU; i++) {
		while (!ps_load((unsigned long *)&cdone[i])) ;
	}

	PRINTC("Done\n");
	perfdata_init(&pd, "INT");
	capmgr_hw_periodic_attach(HW_HPET_PERIODIC, sl_thd_thdid(ht), TEST_INT_PERIOD_US);

}

void
cos_init(void)
{
	int i;
	static unsigned long first = NUM_CPU + 1, init_done[NUM_CPU] = { 0 };
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci    = cos_compinfo_get(defci);

	if (ps_cas(&first, NUM_CPU + 1, cos_cpuid())) {
		cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
		cos_defcompinfo_init();

	} else {
		while (!ps_load(&init_done[first])) ;

		cos_defcompinfo_sched_init();
	}
	ps_faa(&init_done[cos_cpuid()], 1);
	for (i = 0; i < NUM_CPU; i++) {
		while (!ps_load(&init_done[i])) ;
	}

	sl_init(SCHED_PERIOD_US);
	hypercall_comp_init_done();
#ifdef TEST_IPC
	test_ipc_setup();
#endif
#ifdef TEST_INT
	test_int_setup();
#endif

	PRINTC("Starting scheduling\n");
	sl_sched_loop_nonblock();

	assert(0);

	return;
}

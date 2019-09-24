/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#include <sl.h>
#include <res_spec.h>
#include <hypercall.h>
#include <sched_info.h>
#include <crt_chan.h>
#include <chan_crt.h>
#include <channel.h>

u32_t cycs_per_usec = 0;
cycles_t *int_start = NULL;

void
sched_child_init(struct sched_childinfo *schedci)
{
	vaddr_t dcbaddr;
	struct sl_thd *initthd;

	assert(schedci);
	assert(!(schedci->flags & COMP_FLAG_SCHED));
	schedci->initthd = sl_thd_initaep_alloc(sched_child_defci_get(schedci), NULL, 0, 0, 0, 0, 0, &dcbaddr);
        assert(schedci->initthd);
	initthd = schedci->initthd;

	sl_thd_param_set(initthd, sched_param_pack(SCHEDP_PRIO, 1));
}

extern void __sched_stdio_thd_init(thdid_t, struct crt_chan *, struct crt_chan *);
#define MAX_PIPE_SZ 4
CRT_CHAN_STATIC_ALLOC(c0, CHAN_CRT_ITEM_TYPE, CHAN_CRT_NSLOTS);
CRT_CHAN_STATIC_ALLOC(c1, CHAN_CRT_ITEM_TYPE, CHAN_CRT_NSLOTS);
CRT_CHAN_STATIC_ALLOC(c2, CHAN_CRT_ITEM_TYPE, CHAN_CRT_NSLOTS);
CRT_CHAN_STATIC_ALLOC(c3, CHAN_CRT_ITEM_TYPE, CHAN_CRT_NSLOTS);

#define SPDID_INT 1
#define SPDID_W1  3
#define SPDID_W3  5

#define PRIO_INT  MAX_PIPE_SZ + 1
#define PRIO_W0   MAX_PIPE_SZ + 1 - 1
#define PRIO_W1   MAX_PIPE_SZ + 1 - 2
#define PRIO_W2   MAX_PIPE_SZ + 1 - 3
#define PRIO_W3   MAX_PIPE_SZ + 1 - 4

#define SND_DATA 0x1234

#define SHMCHANNEL_KEY 0x2020
#define MAX_ITERS 100
int iters = 0;
cycles_t tot = 0, wc = 0;

static void
work_thd_fn(void *data)
{
	int is_last = (int)data;
	unsigned long i = 0;

	while (1) {
		i = chan_in();
		if (unlikely(is_last)) {
			//printc("[E%u]", cos_thdid());
			cycles_t end, diff;
			rdtscll(end);
			assert(int_start);
			diff = end - *int_start;
			if (wc < diff) wc = diff;
			tot += diff;
			iters++;

			if (iters == MAX_ITERS) {
				printc("%llu, %llu\n", tot / iters, wc);
				tot = wc = 0;
				iters = 0;
			}
			continue;
		} else {
			//printc("[W%u]", cos_thdid());
		}
		chan_out(SND_DATA);
	}
}

thdid_t
sched_child_thd_create(struct sched_childinfo *schedci, thdclosure_index_t idx)
{
	vaddr_t addr;
	struct sl_thd *t = sl_thd_aep_alloc_ext(sched_child_defci_get(schedci), NULL, idx, 0, 0, 0, 0, 0, &addr, NULL);
	assert(t);
	if (cos_inv_token() == SPDID_W1) {
		sl_thd_param_set(t, sched_param_pack(SCHEDP_PRIO, PRIO_W1));
		__sched_stdio_thd_init(sl_thd_thdid(t), c1, c2);
	} else if (cos_inv_token() == SPDID_W3) {
		sl_thd_param_set(t, sched_param_pack(SCHEDP_PRIO, PRIO_W3));
		__sched_stdio_thd_init(sl_thd_thdid(t), c3, NULL);
	}

	return t ? sl_thd_thdid(t) : 0;
}

thdid_t
sched_child_aep_create(struct sched_childinfo *schedci, thdclosure_index_t idx, int owntc, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax, arcvcap_t *extrcv)
{
	assert(cos_inv_token() == SPDID_INT);
	int first = 1;
	vaddr_t addr;
	/* only 1 aep */
	if (!ps_cas(&first, 1, 0)) assert(0);
	struct sl_thd *t = sl_thd_aep_alloc_ext(sched_child_defci_get(schedci), NULL, idx, 1, owntc, key, ipiwin, ipimax, &addr, extrcv);
	assert(t);
	sl_thd_param_set(t, sched_param_pack(SCHEDP_PRIO, PRIO_INT));
	__sched_stdio_thd_init(sl_thd_thdid(t), NULL, c0);

	return t ? sl_thd_thdid(t) : 0;
}

void
test_pipes_init(void)
{
	struct sl_thd *t = sl_thd_alloc(work_thd_fn, 1);
	assert(t);
	sl_thd_param_set(t, sched_param_pack(SCHEDP_PRIO, PRIO_W0));
	__sched_stdio_thd_init(sl_thd_thdid(t), c0, NULL);
	//__sched_stdio_thd_init(sl_thd_thdid(t), c0, c1);
//	t = sl_thd_alloc(work_thd_fn, 0);
//	assert(t);
//	sl_thd_param_set(t, sched_param_pack(SCHEDP_PRIO, PRIO_W2));
//	__sched_stdio_thd_init(sl_thd_thdid(t), c2, c3);
}

void
cos_init(void)
{
	struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
	struct cos_compinfo    *ci    = cos_compinfo_get(defci);
	static volatile unsigned long first = NUM_CPU + 1, init_done[NUM_CPU] = { 0 };
	static u32_t cpubmp[NUM_CPU_BMP_WORDS] = { 0 };
	int i;

	PRINTLOG(PRINT_DEBUG, "CPU cycles per sec: %u\n", cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE));

	if (ps_cas((unsigned long *)&first, NUM_CPU + 1, cos_cpuid())) {
		cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
		cos_defcompinfo_init();
		cos_init_args_cpubmp(cpubmp);
	} else {
		while (!ps_load((unsigned long *)&init_done[first])) ;

		cos_defcompinfo_sched_init();
	}
	ps_faa((unsigned long *)&init_done[cos_cpuid()], 1);

	/* make sure the INITTHD of the scheduler is created on all cores.. for cross-core sl initialization to work! */
	for (i = 0; i < NUM_CPU; i++) {
		if (!bitmap_check(cpubmp, i)) continue;

		while (!ps_load((unsigned long *)&init_done[i])) ;
	}

	sl_init_corebmp(100*SL_MIN_PERIOD_US, cpubmp);
	vaddr_t tscaddr = 0;
	cbuf_t id = channel_shared_page_alloc(SHMCHANNEL_KEY, &tscaddr);
	assert(id >= 0);
	int_start = (cycles_t *)tscaddr;
	*int_start = 0ULL;
	sched_childinfo_init();
	test_pipes_init();
	self_init[cos_cpuid()] = 1;
	hypercall_comp_init_done();

	sl_sched_loop_nonblock();

	PRINTLOG(PRINT_ERROR, "Should never have reached this point!!!\n");
	assert(0);
}

#include <cos_kernel_api.h>
#include <cos_types.h>
#include <ck_ring.h>
#include <inplace_ring.h>
#include <sl.h>
#include <channel.h>
#include <capmgr.h>

static asndcap_t cfesnd    = 0;
static cbuf_t    cfeshmid  = 0;
static vaddr_t   cfeshaddr = NULL;
static struct sl_thd *cfehpetthd = NULL;
static struct ck_ring *cfering = NULL;

/* TODO: share these macros with the cFE component */
#define HPET_PRIO 1
#define HPET_PERIOD_US (500*1000)
#define HPET_SKIP_N 20
INPLACE_RING_BUILTIN(cycles, cycles_t);

void
hpet_thd_fn(arcvcap_t r, void *d)
{
	int first = 1;
	unsigned long counter = 0, sent = 0;

	while (1) {
		cycles_t st_time = 0;
		int pending, ret, rcvd = 0;

		pending = cos_rcv(r, RCV_ALL_PENDING, &rcvd);
		assert(pending == 0 && rcvd == 1);

		rdtscll(st_time);

		if (unlikely(!cfesnd)) cfesnd = capmgr_asnd_key_create(CFE_HPET_SH_KEY);
		if (unlikely(!cfesnd)) continue;

		if (unlikely(first)) {
			counter++;
			if (counter == HPET_SKIP_N) first = 0;
			else continue;
		}

		while (rcvd > 0) {
			/* add the timer activation time to the ring */
			ret = inplace_ring_enq_spsc_cycles(cfeshaddr, cfering, &st_time);
			assert(ret == true);

			assert(cfesnd);
			sent++;
			/* yield because this is obviously highest prio thread */
			cos_asnd(cfesnd, 1);
			rcvd--;
		}
	}
}

void
hpet_attach(void)
{
	int ret;

	if ((ret = cos_tcap_transfer(sl_thd_rcvcap(cfehpetthd), BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, HPET_PRIO))) {
		PRINTC("Failed to transfer INF budget\n");
		assert(0);
	}

	sl_thd_param_set(cfehpetthd, sched_param_pack(SCHEDP_WINDOW, HPET_PERIOD_US));
	sl_thd_param_set(cfehpetthd, sched_param_pack(SCHEDP_PRIO, HPET_PRIO));
	ret = capmgr_hw_periodic_attach(HW_HPET_PERIODIC, sl_thd_thdid(cfehpetthd), HPET_PERIOD_US);
	assert(ret == 0);
}

void
hpet_thd_init(void)
{
	assert(cfeshmid && cfeshaddr && cfering);

	cfehpetthd = sl_thd_aep_alloc(hpet_thd_fn, NULL, 1, 0, 0, 0);
	assert(cfehpetthd);

	PRINTC("Done initializing HPET THREAD\n");
}

void
hpet_ring_init(void)
{
	cfeshmid = channel_shared_page_allocn(CFE_HPET_SH_KEY, CFE_HPET_RING_NPAGES, &cfeshaddr);
	assert(cfeshmid && cfeshaddr);

	memset((void *)cfeshaddr, 0, CFE_HPET_RING_NPAGES * PAGE_SIZE);

	cfering = inplace_ring_init_cycles(cfeshaddr, ((CFE_HPET_RING_NPAGES - 1) * PAGE_SIZE) + sizeof(struct ck_ring));
	PRINTC("Done initializing HPET RING, cbuf ID:%u\n", cfeshmid);
}

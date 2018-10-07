#include <cos_types.h>
#include <cos_kernel_api.h>
#include <capmgr.h>
#include <sl.h>

#define RCV_CORE 0
#define SND_CORE_ST 2 /* 2 to NUM_CPU */

#define RCV_PRIO 30
#define SND_PRIO 1

#define TEST_PERIOD_US 10000

#define IPI_RATE_SET

#ifdef IPI_RATE_SET
#define IPI_RATE 1
#define IPI_PERIOD_US (500*1000) //rate at which sensors trigger in cFE OSK..
#else
#define IPI_RATE 0
#define IPI_PERIOD_US 0
#endif

#define TEST_WAIT_TIME_US (20*1000*1000) //50secs

static volatile arcvcap_t c0_rcvs[NUM_CPU-SND_CORE_ST] CACHE_ALIGNED = { 0 };
static volatile asndcap_t cn_snds[NUM_CPU-SND_CORE_ST] CACHE_ALIGNED = { 0 };

static void
rcv_fn(arcvcap_t r, void *d)
{
	while (1) {
		int pending, rcvd;

		pending = cos_rcv(r, RCV_ALL_PENDING, &rcvd);
		assert(0); /* this should not activate.. is lower prio than anything else! */
		assert(pending == 0 && rcvd >= 1);
	}
}

static void
snd_fn(arcvcap_t r, void *d)
{
	asndcap_t s = cn_snds[NUM_CPU-cos_cpuid()-1];

	PRINTC("Waiting for %llu usecs\n", (cycles_t)TEST_WAIT_TIME_US);
	sl_thd_block_timeout(0, sl_now() + sl_usec2cyc(TEST_WAIT_TIME_US));

	while (1) {
		int ret = cos_asnd(s, 0);

		/* 0 on success.. -EBUSY if kernel fails to enq to ipi ring.. -EDQUOT if capmgr rate-limit triggers.. */
		assert(ret == 0 || ret == -EBUSY || ret == -EDQUOT);
	}
}

void
ipi_test_init(void)
{
	PRINTC("Setting up IPI IF threads..");
	/* using cores 2 to N for sending.. don't use cfe or rk cores! */
	assert(NUM_CPU >= 3);

	if (cos_cpuid() == 0) { /* BETTER BE CFE HERE */
		struct sl_thd *rcv_thds[NUM_CPU-SND_CORE_ST] = { NULL };
		int i, ret;

		for (i = 0; i < NUM_CPU-SND_CORE_ST; i++) {
			PRINTC("Creating %d rcv thread\n", i);
			rcv_thds[i] = sl_thd_aep_alloc(rcv_fn, NULL, 1, 0, IPI_PERIOD_US, IPI_RATE);
			assert(rcv_thds[i]);

			if ((ret = cos_tcap_transfer(sl_thd_rcvcap(rcv_thds[i]), BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, TCAP_PRIO_MIN))) {
				PRINTC("FAILED TO TRANSFER INF BUDGET\n");
				assert(0);
			}
			/* perhaps not register for scheduling??, all we need is rcvcaps.. no scheduling required.. we'll know if the kernel decides to run anyway! */
			/* we don't want these threads to run if cFE has nothing to run so I think it is probably correct to not register! */
			//sl_thd_param_set(rcv_thds[i], sched_param_pack(SCHEDP_WINDOW, TEST_PERIOD_US));
			//sl_thd_param_set(rcv_thds[i], sched_param_pack(SCHEDP_PRIO, RCV_PRIO));

			c0_rcvs[i] = sl_thd_rcvcap(rcv_thds[i]);
			assert(c0_rcvs[i]);
			PRINTC("Done..\n");
			while (cn_snds[i] == 0) ;
			PRINTC("Double Done..\n");
		}
	} else if (cos_cpuid() >= SND_CORE_ST) {
		PRINTC("Creating snd thread\n");
		struct sl_thd *snd_thd = NULL;
		int i = NUM_CPU - cos_cpuid() - 1, ret;

		snd_thd = sl_thd_aep_alloc(snd_fn, NULL, 1, 0, 0, 0);
		assert(snd_thd);

		if ((ret = cos_tcap_transfer(sl_thd_rcvcap(snd_thd), BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, TCAP_RES_INF, SND_PRIO))) {
			PRINTC("FAILED TO TRANSFER INF BUDGET\n");
			assert(0);
		}
		sl_thd_param_set(snd_thd, sched_param_pack(SCHEDP_WINDOW, TEST_PERIOD_US));
		sl_thd_param_set(snd_thd, sched_param_pack(SCHEDP_PRIO, SND_PRIO));

		PRINTC("Done..\n");
		while (c0_rcvs[i] == 0) ;

		cn_snds[i] = capmgr_asnd_rcv_create(c0_rcvs[i]);
		assert(cn_snds[i]);
		PRINTC("Double Done..\n");
	}
	PRINTC("Done!\n");
}

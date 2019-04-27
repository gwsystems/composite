#include <stdint.h>

#include "micro_xcores.h"

/* only one of the following tests must be enable at a time */
/* each core snd to all other cores through N threads.. and rcv from n threads.. */

extern unsigned int cyc_per_usec;

static volatile arcvcap_t test_rcvs[NUM_CPU][NUM_CPU];
static volatile asndcap_t test_asnds[NUM_CPU][NUM_CPU];
static volatile thdcap_t  test_rthds[NUM_CPU][NUM_CPU];
static volatile thdid_t   test_rtids[NUM_CPU][NUM_CPU];
static volatile int       test_thd_blkd[NUM_CPU][NUM_CPU];
#define MIN_THRESH 1000

static void
test_ipi_fn(void *d)
{
        asndcap_t snd = test_asnds[cos_cpuid()][(int)d];
        arcvcap_t rcv = test_rcvs[cos_cpuid()][(int)d];

        assert(snd && rcv);
        while (1) {
                int r = 0, p = 0;

                r = cos_asnd(snd, 1);
                assert(r == 0);
                p = cos_rcv(rcv, RCV_ALL_PENDING, &r);
                assert(p >= 0);
        }
}

static void
test_rcv_crt(void)
{
        int i;
        static volatile int rcv_crt[NUM_CPU] = { 0 };

        memset((void *)test_rcvs[cos_cpuid()], 0, NUM_CPU * sizeof(arcvcap_t));
        memset((void *)test_asnds[cos_cpuid()], 0, NUM_CPU * sizeof(asndcap_t));
        memset((void *)test_rthds[cos_cpuid()], 0, NUM_CPU * sizeof(thdcap_t));
        memset((void *)test_rtids[cos_cpuid()], 0, NUM_CPU * sizeof(thdid_t));
        memset((void *)test_thd_blkd[cos_cpuid()], 0, NUM_CPU * sizeof(int));

        for (i = 0; i < NUM_CPU; i++) {
                thdcap_t thd = 0;
                arcvcap_t rcv = 0;
                asndcap_t snd = 0;

                if (cos_cpuid() == i) continue;
                thd = cos_thd_alloc(&booter_info, booter_info.comp_cap, test_ipi_fn, (void *)i);
                assert(thd);

                rcv = cos_arcv_alloc(&booter_info, thd, BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, booter_info.comp_cap, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE);
                assert(rcv);

                test_rcvs[cos_cpuid()][i] = rcv;
                test_rthds[cos_cpuid()][i] = thd;
                test_rtids[cos_cpuid()][i] = cos_introspect(&booter_info, thd, THD_GET_TID);
        }
        rcv_crt[cos_cpuid()] = 1;

        /* wait for rcvcaps to be created on all cores */
        for (i = 0; i < NUM_CPU; i++) {
                while (!rcv_crt[i]) ;
        }
}

static int
test_find_tid(thdid_t tid)
{
        int i = 0, r = -1;

        for (i = 0; i < NUM_CPU; i++) {
                if (test_rtids[cos_cpuid()][i] != tid) continue;

                r = i;
                break;
        }
        return r;
}

static void
test_asnd_crt(void)
{
        int i;
        static volatile int snd_crt[NUM_CPU] = { 0 };

        for (i = 0; i < NUM_CPU; i++) {
                arcvcap_t rcv = 0;
                asndcap_t snd = 0;

                if (i == cos_cpuid()) continue;
                rcv = test_rcvs[i][cos_cpuid()];
                snd = cos_asnd_alloc(&booter_info, rcv, booter_info.captbl_cap);
                assert(snd);

                test_asnds[cos_cpuid()][i] = snd;
        }
        snd_crt[cos_cpuid()] = 1;

        /* wait for sndcaps to be created on all cores for all cores */
        for (i = 0; i < NUM_CPU; i++) {
                while (!snd_crt[i]) ;
        }
}

static void
test_thd_act(void)
{
        int i, ret;

        for (i = 0; i < NUM_CPU; i ++) {
                if (i == cos_cpuid()) continue;
                if (test_thd_blkd[cos_cpuid()][i]) continue;

                do {
                        ret = cos_switch(test_rthds[cos_cpuid()][i], BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, 0, 0, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, cos_sched_sync());
                } while (ret == -EAGAIN);
                if (ret == -EBUSY) break;
        }
}

void
test_ipi_n_n(void)
{
        int i;
        volatile cycles_t now, prev, total = 0, wc = 0;
        unsigned long *blk[NUM_CPU];

        if (NUM_CPU == 1) {
                blk[cos_cpuid()] = NULL;

                return;
        }

        test_rcv_crt();
        test_asnd_crt();

        for (i = 0; i < NUM_CPU; i++) {
                if (i == cos_cpuid()) blk[i] = NULL;
                else blk[i] = (unsigned long *)&test_thd_blkd[cos_cpuid()][i];
        }
        PRINTC("Start scheduling the threads on this core\n");

        rdtscll(now);
        prev = now;
        while (1) {
                int blocked, rcvd, pending;
                cycles_t cycles;
                tcap_time_t timeout, thd_timeout;
                thdid_t tid;
                int j;

                rdtscll(now);
                if (now - prev > MIN_THRESH) total += now - prev;
                if (now - prev > wc) wc = now - prev;
                test_thd_act();

                while ((pending = cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, RCV_ALL_PENDING, 0,
                                 &rcvd, &tid, &blocked, &cycles, &thd_timeout)) >= 0) {
                        if (!tid) goto done;
                        j = test_find_tid(tid);
                        assert(j >= 0);

                        assert(blk[j]);
                        *(blk[j]) = blocked;

done:                   if(!pending) break;
                }
                rdtscll(prev);
        }

        assert(0);
}

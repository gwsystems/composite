#ifndef SIMPLE_SL_H
#define SIMPLE_SL_H

#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <cos_types.h>

typedef enum {
	SIMPLE_SL_THD_RUNNING,
	SIMPLE_SL_THD_BLOCKED,
} simple_sl_thd_state_t;

struct simple_sl_thd {
	simple_sl_thd_state_t state;
	tcap_prio_t         prio;
	struct cos_aep_info aep;
} CACHE_ALIGNED;

#define SIMPLE_SL_MAXTHDS 32
static struct simple_sl_thd simple_sl_ti[NUM_CPU][SIMPLE_SL_MAXTHDS];

static inline struct simple_sl_thd *
simple_sl_thd_thd(thdid_t tid)
{ return &simple_sl_ti[cos_cpuid()][tid]; }

static inline thdid_t
simple_sl_thd_tid(struct simple_sl_thd *t)
{ return t->aep.tid; }

static inline thdcap_t
simple_sl_thd_thdc(struct simple_sl_thd *t)
{ return t->aep.thd; }

static inline thdid_t
simple_sl_thd_tcap(struct simple_sl_thd *t)
{ return t->aep.tc; }

static inline thdid_t
simple_sl_thd_rcv(struct simple_sl_thd *t)
{ return t->aep.rcv; }

static inline simple_sl_thd_state_t
simple_sl_thd_state(struct simple_sl_thd *t)
{ return t->state; }

static inline void
simple_sl_thd_state_set(struct simple_sl_thd *t, simple_sl_thd_state_t s)
{ t->state = s; }

static inline void
simple_sl_init(void)
{
	memset(&simple_sl_ti[cos_cpuid()], 0, sizeof(struct simple_sl_thd) * SIMPLE_SL_MAXTHDS);
}

static inline struct simple_sl_thd *
simple_sl_thd_init(struct cos_aep_info *aep, tcap_prio_t p)
{
	struct simple_sl_thd *t = NULL;

	assert(aep && aep->thd && aep->tid && aep->tid < SIMPLE_SL_MAXTHDS);
	t = simple_sl_thd_thd(aep->tid);
	assert(simple_sl_thd_tid(t) == 0);
	t->state = SIMPLE_SL_THD_RUNNING;
	t->prio  = p;
	memcpy(&t->aep, aep, sizeof(struct cos_aep_info));

	return t;
}

static inline struct simple_sl_thd *
simple_sl_thd_init_ext(thdid_t tid, tcap_t tc, thdcap_t thd, arcvcap_t rcv, tcap_prio_t p)
{
	struct cos_aep_info *aep = NULL;
	struct simple_sl_thd *t = NULL;

	assert(tid && thd);
	assert(tc == 0 || tc == BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE);
	t = simple_sl_thd_thd(tid);
	assert(simple_sl_thd_tid(t) == 0);
	aep = &t->aep;
	t->state = SIMPLE_SL_THD_RUNNING;
	aep->thd = thd;
	aep->tid = tid;
	aep->rcv = rcv;
	aep->tc  = tc;
	t->prio  = p;

	return t;
}

static inline void
simple_sl_thd_exit(void)
{
	struct simple_sl_thd *t = simple_sl_thd_thd(cos_thdid());

	assert(t && simple_sl_thd_tid(t));
	/* there is a potential race.. but well, a simple sched.. */
	memset(t, 0, sizeof(struct simple_sl_thd));
}

static inline void
simple_sl_schedule(void)
{
	struct simple_sl_thd *t = NULL;
	int i = 1;
	int ret = 0;

	/* cyclic scheduling */
	while (i < SIMPLE_SL_MAXTHDS) {
		i++;
		t = simple_sl_thd_thd(i);
		if (unlikely(simple_sl_thd_tid(t) == 0)) continue;
		if (likely(t->state == SIMPLE_SL_THD_RUNNING)) goto sched;
	}
	if (likely(!t || simple_sl_thd_tid(t) == 0)) return;

sched:
	assert(t && simple_sl_thd_tid(t));
	do {
		ret = cos_switch(simple_sl_thd_thdc(t), BOOT_CAPTBL_SELF_INITTCAP_CPU_BASE, 0, 0, BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, cos_sched_sync());
	} while (ret == -EAGAIN);
}

static inline void
simple_sl_sched_loop_nonblock(void)
{
	while (1) {
		struct simple_sl_thd *t = NULL;
		int blocked, rcvd, pending, ret;
		cycles_t cycles;
		tcap_time_t timeout, thd_timeout;
		thdid_t thdid;

		while ((pending = cos_sched_rcv(BOOT_CAPTBL_SELF_INITRCV_CPU_BASE, RCV_ALL_PENDING | RCV_NON_BLOCKING, 0,
						&rcvd, &thdid, &blocked, &cycles, &thd_timeout)) >= 0) {
			if (!thdid) goto done;
			t = simple_sl_thd_thd(thdid);
			if (simple_sl_thd_tid(t) == 0) goto done;
			simple_sl_thd_state_set(t, blocked);
done:
			if (!pending) break;
		}

		simple_sl_schedule();
	}
}

#endif /* SIMPLE_SL_H */

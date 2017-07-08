#include <sl.h>
#include <sl_consts.h>
#include <sl_plugins.h>
#include <heap.h>

#define SL_TIMEOUT_MOD_MAX_THDS MAX_NUM_THREADS
#define SL_TIMEOUT_HEAP()       (&timeout_heap.h)

struct timeout_heap {
	struct heap  h;
	void        *data[SL_TIMEOUT_MOD_MAX_THDS];
} timeout_heap;

/* wakeup any blocked threads! */
void
sl_timeout_mod_wakeup_expired(cycles_t now)
{
	if (!heap_size(SL_TIMEOUT_HEAP())) return;

	do {
		struct sl_thd *tp, *th;

		tp = heap_peek(SL_TIMEOUT_HEAP());
		assert(tp);

		/* FIXME: logic for wraparound in current tsc */
		if (tp->timeout_cycs > now) break;

		th = heap_highest(SL_TIMEOUT_HEAP());
		assert(th && th == tp);
		th->timeout_idx = -1;

		sl_thd_wakeup_no_cs(th);
	} while (heap_size(SL_TIMEOUT_HEAP()));
}

void
sl_timeout_mod_block(struct sl_thd *t, cycles_t timeout)
{
	assert(t && t->timeout_idx == -1); /* not already in heap */
	assert(heap_size(SL_TIMEOUT_HEAP()) < SL_TIMEOUT_MOD_MAX_THDS);

	if (!timeout) {
		cycles_t tmp = t->timeout_cycs;

		assert(t->period);
		t->timeout_cycs += t->period; /* implicit timeout = task period */
		assert(tmp < t->timeout_cycs); /* wraparound check */
	} else {
		t->timeout_cycs  = timeout;
	}

	heap_add(SL_TIMEOUT_HEAP(), t);
}

void
sl_timeout_mod_expended(microsec_t now, microsec_t oldtimeout)
{
	cycles_t offset;

	assert(now >= oldtimeout);

	/* in virtual environments, or with very small periods, we might miss more than one period */
	offset = (now - oldtimeout) % sl_timeout_period_get();
	sl_timeout_oneshot(now + sl_timeout_period_get() - offset);
}

static int
__compare_min(void *a, void *b)
{
	/* FIXME: logic for wraparound in either timeout_cycs */
	return ((struct sl_thd *)a)->timeout_cycs <= ((struct sl_thd *)b)->timeout_cycs;
}

static void
__update_idx(void *e, int pos)
{ ((struct sl_thd *)e)->timeout_idx = pos; }

void
sl_timeout_mod_init(void)
{
	sl_timeout_period(SL_PERIOD_US);
	heap_init(SL_TIMEOUT_HEAP(), SL_TIMEOUT_MOD_MAX_THDS, __compare_min, __update_idx);
}

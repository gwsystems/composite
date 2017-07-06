#include <sl.h>
#include <sl_consts.h>
#include <sl_plugins.h>
#include <heap.h>

#define SL_TIMEOUT_MOD_MAX_THDS MAX_NUM_THREADS

struct wakeup_heap {
	struct heap  h;
	void        *data[SL_TIMEOUT_MOD_MAX_THDS];
	char         p; /* pad. TODO: use alignment */
} wakeup_heap;

static struct heap *wkh = (struct heap *)&wakeup_heap;

/* wakeup any blocked threads! */
static void
__sl_timeout_mod_wakeup_expired(cycles_t now)
{
	if (!heap_size(wkh)) return;

	do {
		struct sl_thd *tp, *th;

		tp = heap_peek(wkh);
		assert(tp);

		if (tp->wakeup_cycs > now) break;

		th = heap_highest(wkh);
		assert(th && th == tp);
		th->wakeup_idx = -1;
		sl_thd_wakeup_no_cs(th);
	} while (heap_size(wkh));
}

void
sl_timeout_mod_block(struct sl_thd *t, cycles_t wakeup)
{
	assert(t && t->wakeup_idx == -1); /* not already in heap */
	assert(heap_size(wkh) < SL_TIMEOUT_MOD_MAX_THDS);

	if (!wakeup) t->wakeup_cycs += t->period; /* implicit wakeup = task period */
	else         t->wakeup_cycs  = wakeup;

	heap_add(wkh, t);
}

void
sl_timeout_mod_wakeup_expired(cycles_t now)
{ __sl_timeout_mod_wakeup_expired(now); }

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
{ return ((struct sl_thd *)a)->wakeup_cycs <= ((struct sl_thd *)b)->wakeup_cycs; }

static void
__update_idx(void *e, int pos)
{ ((struct sl_thd *)e)->wakeup_idx = pos; }

void
sl_timeout_mod_init(void)
{
	sl_timeout_period(SL_PERIOD_US);
	heap_init(wkh, SL_TIMEOUT_MOD_MAX_THDS, __compare_min, __update_idx);
}

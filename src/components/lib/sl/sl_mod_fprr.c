#include <sl.h>
#include <sl_consts.h>
#include <sl_mod_policy.h>
#include <sl_plugins.h>

#define SL_FPRR_NPRIOS         32
#define SL_FPRR_PRIO_HIGHEST   TCAP_PRIO_MAX
#define SL_FPRR_PRIO_LOWEST    SL_FPRR_NPRIOS

#define SL_FPRR_PERIOD_US_MIN  SL_MIN_PERIOD_US

static unsigned int thdlist_bmp[NUM_CPU] CACHE_ALIGNED;
static struct ps_list_head threads[NUM_CPU][SL_FPRR_NPRIOS] CACHE_ALIGNED;

void
sl_mod_execution(struct sl_thd_policy *t, cycles_t cycles)
{ }

struct sl_thd_policy *
sl_mod_schedule(void)
{
	int i;
	struct sl_thd_policy *t = NULL;

	if (unlikely(!thdlist_bmp[cos_cpuid()])) return NULL;
	i = __builtin_ctz(thdlist_bmp[cos_cpuid()]);
	assert(i < SL_FPRR_NPRIOS);
	assert(!ps_list_head_empty(&threads[cos_cpuid()][i]));
	t = ps_list_head_first_d(&threads[cos_cpuid()][i], struct sl_thd_policy);
	assert(t);

	ps_list_rem_d(t);
	ps_list_head_append_d(&threads[cos_cpuid()][i], t);

	return t;
}

static inline void
__sl_mod_bmp_unset(struct sl_thd_policy *t)
{
	unsigned int ctb = ps_load(&thdlist_bmp[cos_cpuid()]);
	unsigned int p = t->priority - 1, b = 1 << p;

	if (!ps_list_head_empty(&threads[cos_cpuid()][p])) return;

	/* unset from bitmap if there are no threads at this priority */
	if (unlikely(!ps_upcas(&thdlist_bmp[cos_cpuid()], ctb, ctb & ~b))) assert(0);
}

static inline void
__sl_mod_bmp_set(struct sl_thd_policy *t)
{
	unsigned int ctb = ps_load(&thdlist_bmp[cos_cpuid()]);
	unsigned int p = t->priority - 1, b = 1 << p;

	if (unlikely(ctb & b)) return; 

	assert(!ps_list_head_empty(&threads[cos_cpuid()][p]));
	/* set to bitmap if this is the first element added at this prio! */
	if (unlikely(!ps_upcas(&thdlist_bmp[cos_cpuid()], ctb, ctb | b))) assert(0);
}

void
sl_mod_block(struct sl_thd_policy *t)
{
	ps_list_rem_d(t);
	__sl_mod_bmp_unset(t);
}

void
sl_mod_wakeup(struct sl_thd_policy *t)
{
	assert(ps_list_singleton_d(t));
	ps_list_head_append_d(&threads[cos_cpuid()][t->priority - 1], t);
	__sl_mod_bmp_set(t);
}

void
sl_mod_yield(struct sl_thd_policy *t, struct sl_thd_policy *yield_to)
{
	ps_list_rem_d(t);
	ps_list_head_append_d(&threads[cos_cpuid()][t->priority - 1], t);
}

void
sl_mod_thd_create(struct sl_thd_policy *t)
{
	t->priority    = SL_FPRR_PRIO_LOWEST;
	t->period      = 0;
	t->period_usec = 0;
	sl_thd_setprio(sl_mod_thd_get(t), t->priority);
	ps_list_init_d(t);
}

void
sl_mod_thd_delete(struct sl_thd_policy *t)
{
	ps_list_rem_d(t); 
	__sl_mod_bmp_unset(t);
}

void
sl_mod_thd_param_set(struct sl_thd_policy *t, sched_param_type_t type, unsigned int v)
{
	switch (type) {
	case SCHEDP_PRIO:
	{
		assert(v >= SL_FPRR_PRIO_HIGHEST && v <= SL_FPRR_PRIO_LOWEST);
		/* should not have been on any prio before, this is FP */
		assert(ps_list_singleton_d(t));
		t->priority = v;
		ps_list_head_append_d(&threads[cos_cpuid()][v - 1], t);
		__sl_mod_bmp_set(t);
		sl_thd_setprio(sl_mod_thd_get(t), v);

		break;
	}
	case SCHEDP_WINDOW:
	{
		assert(v >= SL_FPRR_PERIOD_US_MIN);
		t->period_usec    = v;
		t->period         = sl_usec2cyc(v);
		/* FIXME: synchronize periods for all tasks */

		break;
	}
	case SCHEDP_BUDGET:
	{
		break;
	}
	default: assert(0);
	}
}

void
sl_mod_init(void)
{
	int i;

	thdlist_bmp[cos_cpuid()] = 0;
	memset(threads[cos_cpuid()], 0, sizeof(struct ps_list_head) * SL_FPRR_NPRIOS);
	for (i = 0 ; i < SL_FPRR_NPRIOS ; i++) {
		ps_list_head_init(&threads[cos_cpuid()][i]);
	}
}

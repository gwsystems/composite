#include <sl.h>
#include <sl_consts.h>
#include <sl_mod_policy.h>
#include <sl_plugins.h>

#define SL_FPRR_PERIOD_US_MIN  SL_MIN_PERIOD_US

struct ps_list_head threads[NUM_CPU] CACHE_ALIGNED;

/* No RR yet */
void
sl_mod_execution(struct sl_thd_policy *t, cycles_t cycles)
{ }

struct sl_thd_policy *
sl_mod_schedule(void)
{
	struct sl_thd_policy *t = NULL;

	if (unlikely(ps_list_head_empty(&threads[cos_cpuid()]))) goto done;
	t = ps_list_head_first_d(&threads[cos_cpuid()], struct sl_thd_policy);
	ps_list_rem_d(t);
	ps_list_head_append_d(&threads[cos_cpuid()], t);

done:
	return t;
}

void
sl_mod_block(struct sl_thd_policy *t)
{
	ps_list_rem_d(t);
}

void
sl_mod_wakeup(struct sl_thd_policy *t)
{
	assert(ps_list_singleton_d(t));

	ps_list_head_append_d(&threads[cos_cpuid()], t);
}

void
sl_mod_yield(struct sl_thd_policy *t, struct sl_thd_policy *yield_to)
{
	ps_list_rem_d(t);
	ps_list_head_append_d(&threads[cos_cpuid()], t);
}

void
sl_mod_thd_create(struct sl_thd_policy *t)
{
	t->priority    = TCAP_PRIO_MIN;
	t->period      = 0;
	t->period_usec = 0;
	ps_list_init_d(t);
}

void
sl_mod_thd_delete(struct sl_thd_policy *t)
{ ps_list_rem_d(t); }

void
sl_mod_thd_param_set(struct sl_thd_policy *t, sched_param_type_t type, unsigned int v)
{
	int cpu = cos_cpuid();

	switch (type) {
	case SCHEDP_PRIO:
	{
		t->priority = v;
		sl_thd_setprio(sl_mod_thd_get(t), t->priority);
		ps_list_head_append_d(&threads[cos_cpuid()], t);

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
	ps_list_head_init(&threads[cos_cpuid()]);
}

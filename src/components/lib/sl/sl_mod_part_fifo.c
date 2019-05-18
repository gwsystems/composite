/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2019, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#include <sl.h>
#include <sl_consts.h>
#include <sl_mod_policy.h>
#include <sl_plugins.h>

#define SL_FIFO_PRIO           TCAP_PRIO_MAX
#define SL_FIFO_IDLE_PRIO      SL_FIFO_PRIO+4
#define SL_FIFO_PERIOD_US_MIN  SL_MIN_PERIOD_US

static struct ps_list_head threads[NUM_CPU] CACHE_ALIGNED;
static struct sl_thd_policy *idle_thd[NUM_CPU];

void
sl_mod_execution(struct sl_thd_policy *t, cycles_t cycles)
{ }

struct sl_thd_policy *
sl_mod_schedule(void)
{
	struct sl_thd_policy *c = sl_mod_thd_policy_get(sl_thd_curr());
	struct sl_thd_policy *t = NULL;

	if (unlikely(ps_list_head_empty(&threads[cos_cpuid()]))) goto done;
	t = ps_list_head_first_d(&threads[cos_cpuid()], struct sl_thd_policy);

	return t;
done:
	if (likely(idle_thd[cos_cpuid()])) return idle_thd[cos_cpuid()];

	return t;
}

struct sl_thd_policy *
sl_mod_last_schedule(void)
{
	struct sl_thd_policy *t = NULL;

	if (unlikely(ps_list_head_empty(&threads[cos_cpuid()]))) goto done;
	t = ps_list_head_last_d(&threads[cos_cpuid()], struct sl_thd_policy);

done:
	return t;
}

void
sl_mod_block(struct sl_thd_policy *t)
{
	assert(t != idle_thd[cos_cpuid()]);
	ps_list_rem_d(t);
}

void
sl_mod_wakeup(struct sl_thd_policy *t)
{
	struct sl_thd *tm = sl_mod_thd_get(t);

	assert(t != idle_thd[cos_cpuid()]);
	assert(ps_list_singleton_d(t));

	ps_list_head_append_d(&threads[cos_cpuid()], t);
	/* remove from partlist used for tracking free pool of tasks on this core! */
	if (!ps_list_singleton(tm, partlist)) ps_list_rem(tm, partlist);
}

void
sl_mod_yield(struct sl_thd_policy *t, struct sl_thd_policy *yield_to)
{
	if (unlikely(t == idle_thd[cos_cpuid()])) return;
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

	/* TODO: add to runq here? for now, only add when PRIO is set and that's pretty much it's ARRIVAL time! */
}

void
sl_mod_thd_delete(struct sl_thd_policy *t)
{
	if (unlikely(t == idle_thd[cos_cpuid()])) return;	
	ps_list_rem_d(t);
}

void
sl_mod_thd_param_set(struct sl_thd_policy *t, sched_param_type_t type, unsigned int v)
{
	int cpu = cos_cpuid();

	switch (type) {
	case SCHEDP_PRIO:
	{
		t->priority = v;
		sl_thd_setprio(sl_mod_thd_get(t), t->priority);

		if (v == SL_FIFO_IDLE_PRIO) {
			assert(idle_thd[cos_cpuid()] == NULL);
			idle_thd[cos_cpuid()] = t;
		} else {
			ps_list_head_append_d(&threads[cos_cpuid()], t);
		}

		break;
	}
	case SCHEDP_WINDOW:
	{
		assert(v >= SL_FIFO_PERIOD_US_MIN);
		t->period_usec    = v;
		t->period         = sl_usec2cyc(v);

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
	idle_thd[cos_cpuid()] = NULL;
	ps_list_head_init(&threads[cos_cpuid()]);
}

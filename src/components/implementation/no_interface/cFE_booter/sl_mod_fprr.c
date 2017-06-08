#include <sl.h>
#include <sl_consts.h>
#include <sl_mod_policy.h>
#include <sl_plugins.h>

#define SL_FPRR_NPRIOS  256
#define SL_FPRR_HIGHEST 0
#define SL_FPRR_LOWEST  (SL_FPRR_NPRIOS-1)

struct ps_list_head threads[SL_FPRR_NPRIOS];

/* No RR yet */
void
sl_mod_execution(struct sl_thd_policy *t, cycles_t cycles)
{ }

struct sl_thd_policy *
sl_mod_schedule(void)
{
	int i;
	struct sl_thd_policy *t;

	for (i = 0 ; i < SL_FPRR_NPRIOS ; i++) {
		if (ps_list_head_empty(&threads[i])) continue;
		t = ps_list_head_first_d(&threads[i], struct sl_thd_policy);

		return t;
	}
	assert(0);

	return NULL;
}

void
sl_mod_block(struct sl_thd_policy *t)
{
	ps_list_rem_d(t);
}

void
sl_mod_wakeup(struct sl_thd_policy *t)
{
	assert(t->priority <= SL_FPRR_LOWEST && ps_list_singleton_d(t));

	ps_list_head_append_d(&threads[t->priority], t);
}

void
sl_mod_yield(struct sl_thd_policy *t, struct sl_thd_policy *yield_to)
{
	assert(t->priority <= SL_FPRR_LOWEST);

	ps_list_rem_d(t);
	ps_list_head_append_d(&threads[t->priority], t);
}

void
sl_mod_thd_create(struct sl_thd_policy *t)
{
	t->priority    = SL_FPRR_LOWEST;
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
	assert(type == SCHEDP_PRIO && v < SL_FPRR_NPRIOS);
	ps_list_rem_d(t); 	/* if we're already on a list, and we're updating priority */
	t->priority = v;
	ps_list_head_append_d(&threads[t->priority], t);
}

void
sl_mod_init(void)
{
	int i;
	struct sl_thd *t;

	for (i = 0 ; i < SL_FPRR_NPRIOS ; i++) {
		ps_list_head_init(&threads[i]);
	}
}

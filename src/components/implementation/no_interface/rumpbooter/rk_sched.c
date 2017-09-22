#include <res_spec.h>

#include "rk_sched.h"
#define CS_RECURSE_LIMIT (1<<5)

volatile unsigned int cs_recursive = 0;

static int
rk_rump_thd_param_set(struct sl_thd *t)
{
	union sched_param_union spprio = {.c = {.type = SCHEDP_PRIO, .value = RK_RUMP_THD_PRIO}};

	sl_thd_param_set(t, spprio.v);

	return 0;
}

static int
rk_intr_thd_param_set(struct sl_thd *t, int own_tcap)
{
	union sched_param_union spprio = {.c = {.type = SCHEDP_PRIO, .value = RK_INTR_THD_PRIO}};

	sl_thd_param_set(t, spprio.v);

	assert(own_tcap == 0);
	/* TODO: perhaps set PERIOD & BUDGET if it has it's own TCAP */

	return 0;
}

struct sl_thd *
rk_rump_thd_init(struct cos_aep_info *aep)
{
	struct sl_thd *t = NULL;

	t = sl_thd_init(aep, 0);
	assert(t);	

	rk_rump_thd_param_set(t);

	return t;
}

struct sl_thd *
rk_rump_thd_alloc(cos_thd_fn_t fn, void *data)
{
	struct sl_thd *t = NULL;

	t = sl_thd_alloc(fn, data);
	assert(t);	

	rk_rump_thd_param_set(t);

	return t;
}

struct sl_thd *
rk_intr_aep_alloc(cos_aepthd_fn_t fn, void *data, int own_tcap)
{
	struct sl_thd *t = NULL;

	t = sl_thd_aep_alloc(fn, data, own_tcap);
	assert(t);	

	rk_intr_thd_param_set(t, own_tcap);

	return t;
}

struct sl_thd *
rk_intr_aep_init(struct cos_aep_info *aep, int own_tcap)
{
	struct sl_thd *t = NULL;

	t = sl_thd_init(aep, own_tcap);
	assert(t);	

	rk_intr_thd_param_set(t, own_tcap);

	return t;
}

void
rk_rump_thd_yield_to(struct bmk_thread *c, struct bmk_thread *n)
{
	struct sl_thd *curr = sl_thd_curr();
	thdid_t ntid = get_cos_thdid(n), ctid = get_cos_thdid(c);
	struct sl_thd *t = sl_thd_lkup(ntid);

	assert(ctid == cos_thdid());
	assert(ntid > 0 && t);

	sl_thd_yield(ntid);
}

void
rk_sched_loop(void)
{
	printc("STARTING RK SCHED!\n");
	sl_sched_loop();
}

void
rk_sched_init(microsec_t period)
{
	sl_init(period);
}

void
rk_rump_thd_wakeup(struct bmk_thread *w)
{
	sl_thd_wakeup(get_cos_thdid(w));
}

int
rk_rump_thd_block_timeout(struct bmk_thread *c, unsigned long long timeout)
{
	assert(get_cos_thdid(c) == cos_thdid());

	if (sl_thd_block_timeout(0, timeout)) return 1;

	return 0;
}

void
rk_rump_thd_block(struct bmk_thread *c)
{
	assert(get_cos_thdid(c) == cos_thdid());

	sl_thd_block(0);
}

void
rk_rump_thd_yield(void)
{
	sl_thd_yield(0);
}

void
rk_rump_thd_exit(void)
{
	sl_thd_exit();
}

static void
rk_sched_cs_enter(void)
{
	if (sl_cs_owner()) {
		assert(cs_recursive);
		goto recurse;
	}

	sl_cs_enter();

recurse:
	__sync_add_and_fetch(&cs_recursive, 1);
	assert(cs_recursive < CS_RECURSE_LIMIT); /* make sure it's not taken too many times */
}

static void
rk_sched_cs_exit(void)
{
	assert(sl_cs_owner());

	assert(cs_recursive);

	__sync_sub_and_fetch(&cs_recursive, 1);
	if (!cs_recursive) sl_cs_exit();
}

void
rk_intr_disable(void)
{
	rk_sched_cs_enter();
}

void
rk_intr_enable(void)
{
	rk_sched_cs_exit();
}

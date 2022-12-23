#ifndef SL_MOD_POLICY_H
#define SL_MOD_POLICY_H

#include <sl_thd.h>
#include <ps_list.h>

struct sl_thd_policy {
	struct sl_thd  thd;
	tcap_prio_t    priority;
	microsec_t     period_usec;
	cycles_t       period;
	struct ps_list list;
} CACHE_ALIGNED;

static inline struct sl_thd *
sl_mod_thd_get(struct sl_thd_policy *tp)
{
	return &tp->thd;
}

static inline struct sl_thd_policy *
sl_mod_thd_policy_get(struct sl_thd *t)
{
	return ps_container(t, struct sl_thd_policy, thd);
}

#endif /* SL_MOD_POLICY_H */

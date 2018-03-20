#ifndef SL_CUSTOM_MOD_POLICY_H
#define SL_CUSTOM_MOD_POLICY_H

#include <sl_thd.h>
#include <ps_list.h>

#include "gen/osapi.h"
#include "gen/common_types.h"

struct sl_thd_policy {
	struct sl_thd  thd;
	tcap_prio_t    priority;
	microsec_t     period_usec;
	cycles_t       period;
	struct ps_list list;

	// cFE specific fields
	osal_task_entry delete_handler;
	OS_task_prop_t  osal_task_prop;
};

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

#endif /* SL_CUSTOM_MOD_POLICY_H */

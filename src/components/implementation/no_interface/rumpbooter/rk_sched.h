#ifndef RK_SCHED_H
#define RK_SCHED_H

#include <cos_types.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <sl.h>
#include <sl_thd.h>

#include "vk_types.h"
#include "rumpcalls.h"

#define RK_SCHED_PERIOD_US PARENT_PERIOD_US

#if defined(CHRONOS_ENABLED)
/*
 * RK Scheduler - the system scheduler is
 * aware of chronos transfers to the HA(TIMER) subsystem.
 */
#define TM_SUB_BUDGET_US  (1000) //1ms
#else
#define TM_SUB_BUDGET_US  (3000) //3ms
#endif
#define TM_SUB_PERIOD_US  (10000) //10ms

enum {
	RK_SL_PRIO_HIGH = 5,
	RK_SL_PRIO_MID,
	RK_SL_PRIO_LOW,
};

#define RK_INTR_THD_PRIO RK_SL_PRIO_MID
#define RK_RUMP_THD_PRIO RK_SL_PRIO_LOW
 
struct sl_thd *rk_rump_thd_init(struct cos_aep_info *aep);
struct sl_thd *rk_rump_thd_alloc(cos_thd_fn_t f, void *d);
struct sl_thd *rk_intr_aep_alloc(cos_aepthd_fn_t f, void *d, int own_tcap);
struct sl_thd *rk_intr_aep_init(struct cos_aep_info *aep, int own_tcap);

void rk_sched_init(microsec_t period);
void rk_sched_loop(void);
void rk_rump_thd_yield_to(struct bmk_thread *c, struct bmk_thread *n);

void rk_rump_thd_wakeup(struct bmk_thread *w);
/* 1 if timedout */
int rk_rump_thd_block_timeout(struct bmk_thread *c, unsigned long long timeout);
void rk_rump_thd_block(struct bmk_thread *c);
void rk_rump_thd_yield(void);
void rk_rump_thd_exit(void);
void rk_curr_thd_set_prio(int prio);

void rk_intr_disable(void);
void rk_intr_enable(void);

#endif /* RK_SCHED_H */

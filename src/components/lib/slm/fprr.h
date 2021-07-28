#ifndef FPRR_H
#define FPRR_H

#include <ps_list.h>

struct slm_sched_thd {
	struct ps_list list;
};

#include <slm.h>

void slm_sched_fprr_execution(struct slm_thd *t, cycles_t cycles);
struct slm_thd *slm_sched_fprr_schedule(void);
int slm_sched_fprr_block(struct slm_thd *t);
int slm_sched_fprr_wakeup(struct slm_thd *t);
void slm_sched_fprr_yield(struct slm_thd *t, struct slm_thd *yield_to);
int slm_sched_fprr_thd_init(struct slm_thd *t);
void slm_sched_fprr_thd_deinit(struct slm_thd *t);
int slm_sched_fprr_thd_modify(struct slm_thd *t, sched_param_type_t type, unsigned int v);
void slm_sched_fprr_init(void);

#endif	/* FPRR_H */

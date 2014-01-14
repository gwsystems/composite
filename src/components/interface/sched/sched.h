#ifndef   	SCHED_H
#define   	SCHED_H

#include <res_spec.h>

int sched_wakeup(spdid_t spdid, unsigned short int thd_id);
int sched_block(spdid_t spdid, unsigned short int dependency_thd);

void sched_timeout(spdid_t spdid, unsigned long amnt);
int sched_timeout_thd(spdid_t spdid);
unsigned int sched_tick_freq(void);
unsigned long sched_cyc_per_tick(void);
unsigned long sched_timestamp(void);
unsigned long sched_timer_stopclock(void);
int sched_priority(unsigned short int tid);

/* This function is deprecated...use sched_create_thd instead. */
int sched_create_thread(spdid_t spdid, struct cos_array *data);
int sched_create_thd(spdid_t spdid, u32_t sched_param0, u32_t sched_param1, u32_t sched_param2);
/* Should only be called by the booter/loader */
int sched_create_thread_default(spdid_t spdid, u32_t sched_param_0, u32_t sched_param_1, u32_t sched_param_2);
int sched_thread_params(spdid_t spdid, u16_t thd_id, res_spec_t rs);

int sched_create_net_acap(spdid_t spdid, int acap_id, unsigned short int port);

int sched_component_take(spdid_t spdid);
int sched_component_release(spdid_t spdid);

#endif 	    /* !SCHED_H */

#ifndef   	SCHED_H
#define   	SCHED_H

#include <res_spec.h>

int sched_root_init(void);

int sched_wakeup(spdid_t spdid, unsigned short int thd_id);
int sched_block(spdid_t spdid, unsigned short int dependency_thd);

void sched_timeout(spdid_t spdid, unsigned long amnt);
int sched_timeout_thd(spdid_t spdid);
unsigned int sched_tick_freq(void);
unsigned long sched_timestamp(void);
unsigned long sched_timer_stopclock(void);
int sched_priority(unsigned short int tid);

int sched_create_thread(spdid_t spdid, struct cos_array *data);
int sched_create_thread_default(spdid_t spdid, spdid_t target);
int sched_thread_params(spdid_t spdid, u16_t thd_id, res_spec_t rs);

int sched_create_net_brand(spdid_t spdid, unsigned short int port);
int sched_add_thd_to_brand(spdid_t spdid, unsigned short int bid, unsigned short int tid);


void sched_exit(void);

int sched_component_take(spdid_t spdid);
int sched_component_release(spdid_t spdid);

typedef int cevt_t;
struct sched_child_evt {
	cevt_t t; // type
	unsigned short int tid;
	u64_t time_elapsed;
};

int sched_child_cntl_thd(spdid_t spdid);
int sched_child_thd_crt(spdid_t spdid, spdid_t dest_spd);
int sched_child_get_evt(spdid_t spdid, struct sched_child_evt *e, int idle, unsigned long wake_diff);

#endif 	    /* !SCHED_H */

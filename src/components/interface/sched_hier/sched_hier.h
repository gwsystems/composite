#ifndef   	SCHED_HIER_H
#define   	SCHED_HIER_H

int sched_init(void);
int sched_isroot(void);
void sched_exit(void);

typedef int cevt_t;
struct sched_child_evt {
	cevt_t t; // type
	unsigned short int tid;
	u64_t time_elapsed;
};

int sched_child_cntl_thd(spdid_t spdid);
int sched_child_thd_crt(spdid_t spdid, spdid_t dest_spd);
int sched_child_get_evt(spdid_t spdid, struct sched_child_evt *e, int idle, unsigned long wake_diff);

#endif 	    /* !SCHED_HIER_H */

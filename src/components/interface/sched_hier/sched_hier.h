#ifndef   	SCHED_HIER_H
#define   	SCHED_HIER_H

#include <cos_component.h>

int sched_init(void);
int sched_isroot(void);
void sched_exit(void);

typedef int cevt_t;

int sched_child_cntl_thd(spdid_t spdid);
int sched_child_thd_crt(spdid_t spdid, spdid_t dest_spd);
int sched_child_get_evt(spdid_t spdid, int idle, unsigned long wakediff, cevt_t *t, unsigned short int *tid, u32_t *time_elapsed);

#endif 	    /* !SCHED_HIER_H */

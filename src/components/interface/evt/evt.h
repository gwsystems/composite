#ifndef   	EVT_H
#define   	EVT_H

long evt_create(spdid_t spdid);
void evt_free(spdid_t spdid, long extern_evt);
int evt_wait(spdid_t spdid, long extern_evt);
long evt_grp_wait(spdid_t spdid);
int evt_grp_mult_wait(spdid_t spdid, struct cos_array *data);
int evt_trigger(spdid_t spdid, long extern_evt);
int evt_set_prio(spdid_t spdid, long extern_evt, int prio);
unsigned long *evt_stats(spdid_t spdid, unsigned long *stats);
int evt_stats_len(spdid_t spdid);

#endif 	    /* !EVT_H */

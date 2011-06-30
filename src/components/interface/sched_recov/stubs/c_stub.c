#include <cos_component.h>
#include <sched_recov.h>
#include <cstub.h>

//int sched_create_thread(spdid_t spdid, struct cos_array *data);
CSTUB_2(sched_create_thread, spdid_t, struct cos_array *);
//void sched_timeout(spdid_t spdid, unsigned long amnt);
CSTUB_2(sched_timeout, spdid_t, unsigned long);
//int sched_timeout_thd(spdid_t spdid);
CSTUB_1(sched_timeout_thd, spdid_t);
//int sched_wakeup(spdid_t spdid, unsigned short int thd_id);
CSTUB_2(sched_wakeup, spdid_t, unsigned short int)
//sched_block(spdid_t spdid, unsigned short int dependency_thd);
CSTUB_2(sched_block, spdid_t, unsigned short int);
//sched_component_take(spdid_t spdid);
CSTUB_1(sched_component_take, spdid_t);
//sched_component_release(spdid_t spdid);
CSTUB_1(sched_component_release, spdid_t);


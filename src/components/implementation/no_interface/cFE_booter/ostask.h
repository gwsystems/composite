#ifndef OSTASK_H
#define OSTASK_H

#include <sl.h>

/*
 * ThreadId overrides for apps
 */
extern thdid_t id_overrides[SL_MAX_NUM_THDS];

void OS_SchedulerStart(cos_thd_fn_t main_delegate);

#endif

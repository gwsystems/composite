#ifndef COS_SCHED_DS_H
#define COS_SCHED_DS_H

#include <cos_types.h>

PERCPU_VAR_ATTR(__attribute__((section(".kmem"))), cos_sched_notifications);

#endif

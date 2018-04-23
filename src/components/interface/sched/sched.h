/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#ifndef SCHED_H
#define SCHED_H

#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <sl.h>
#include <res_spec.h>

int      sched_thd_wakeup(thdid_t t);
int      sched_thd_block(thdid_t dep_t);
cycles_t sched_thd_block_timeout(thdid_t dep_t, cycles_t abs_timeout);

thdid_t sched_thd_create(cos_thd_fn_t fn, void *data);
thdid_t sched_aep_create(struct cos_aep_info *aep, cos_aepthd_fn_t fn, void *data, int owntc, cos_channelkey_t key, microsec_t ipiwin, u32_t ipimax);

int sched_thd_param_set(thdid_t tid, sched_param_t p);

int sched_thd_delete(thdid_t tid);
int sched_thd_exit(void);

/* TODO: lock i/f */

#endif /* SCHED_H */

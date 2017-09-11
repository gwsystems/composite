/**
 * Copyright 2010 by The George Washington University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Qi Wang, interwq@gwu.edu, 2011
 */
#ifndef MEM_POOL_H_
#define MEM_POOL_H_

#define NUM_TMEM_MGR 2

#define MAX_NUM_MEM 320

int mempool_put_mem(spdid_t d_spdid, void* mgr_addr);
void *mempool_get_mem(spdid_t spdid, int pages);
int mempool_tmem_mgr_event_waiting(spdid_t spdid);
int mempool_clear_glb_blked(spdid_t spdid);
int mempool_set_mgr_desired(spdid_t spdid, int desired);

#endif

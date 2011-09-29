/**
 * Copyright 2009 by Andrew Sweeney, ajs86@gwu.edu, 2011 Gabriel
 * Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */
#ifndef STKMGR_H_
#define STKMGR_H_

/** 
 * In the future we may want to change this too
 * cos_asm_server_stub_spdid
 */
void *stkmgr_grant_stack(spdid_t d_spdid);
void stkmgr_return_stack(spdid_t s_spdid, vaddr_t addr);

void stkmgr_stack_report(void);
int stkmgr_set_concurrency(spdid_t spdid, int concur_lvl, int remove_spare);
int stkmgr_spd_concurrency_estimate(spdid_t spdid);
unsigned long stkmgr_thd_blk_time(unsigned short int tid, spdid_t spdid, int reset);
int stkmgr_thd_blk_cnt(unsigned short int tid, spdid_t spdid, int reset);
int stkmgr_detect_suspension(spdid_t cid, int reset);
int stkmgr_set_over_quota_limit(int limit);
int stkmgr_set_suspension_limit(spdid_t cid, int limit);
int stkmgr_get_allocated(spdid_t cid);

/* map a stack to the destination location, from the source component */
int stkmgr_stack_introspect(spdid_t d_spdid, vaddr_t d_addr, spdid_t s_spdid, vaddr_t s_addr);
/* unmap a stack that was introspected on */
int stkmgr_stack_close(spdid_t d_spdid, vaddr_t d_addr);

#endif

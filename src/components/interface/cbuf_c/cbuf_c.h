/**
 * Copyright 2010 by The George Washington University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gparmer@gwu.edu, 2010
 */

#ifndef   	CBUF_C_H
#define   	CBUF_C_H

#include <cbuf_vect.h>
#include <mem_mgr_large.h>

/* Component functions */
/* 
 * cbid is complicated:
 * 0: we don't know the cbuf id we want to create
 * n > 0: we want to initialize the cbuf_meta structure and get the cbuf addr
 *
 * The return value is negative (-c) if we need to create a new level
 * in the cbuf_meta vector for c.  We expect that we will call this
 * function again with c to get the cbuf's address via a populated
 * cbuf_meta.
 */
int cbuf_c_create(spdid_t spdid, int size, long cbid); /* return cbid */
int cbuf_c_delete(spdid_t spdid, int cbid);
void *cbuf_c_retrieve(spdid_t spdid, int cbid, int len); /* return client address */


/* CbufMgr API that works with Cbuf_policy*/
int cbufmgr_set_concurrency(spdid_t spdid, int concur_lvl, int remove_spare);
int cbufmgr_spd_concurrency_estimate(spdid_t spdid);
void cbufmgr_buf_report(void);
int cbufmgr_spd_concurrency_estimate(spdid_t spdid);
unsigned long cbufmgr_thd_blk_time(unsigned short int tid, spdid_t spdid, int reset);
int cbufmgr_thd_blk_cnt(unsigned short int tid, spdid_t spdid, int reset);
int cbufmgr_detect_suspension(spdid_t cid, int reset);
int cbufmgr_set_over_quota_limit(int limit);
int cbufmgr_set_suspension_limit(spdid_t cid, int limit);
int cbufmgr_get_allocated(spdid_t cid);

/* 
 * FIXME: The API currently requires the valloc be done in the client,
 * and the cbuf_mgr simply uses this address to map the corresponding
 * memory into.  Later the cbuf_mgr will valloc_free the memory region
 * when it wants the memory back.  This asymmetry is a little
 * worrysome as it is unclear who is "responsible" for tracking the
 * memory.  
 *
 * A solution is to not pass the void *page in this interface, and
 * instead return the vaddrs of the memory that was allocated in
 * cbuf_mgr.  The problem is that for cbuf_c_create, we need to return
 * 2 values in that case.  Thus, we will probably end up passing the
 * cbufid in the page that is returned.  For now, use the current
 * design where both components are using valloc_*.
 */

#endif 	    /* !CBUF_C_H */

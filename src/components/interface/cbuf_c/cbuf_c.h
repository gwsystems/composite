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

#include <cos_component.h>
//#include <cbufp.h>

/* 
 * cbuf_c_create and cbuf_c_retrieve:
 * cbid is complicated --
 * 0: we don't know the cbuf id we want to create
 * n > 0: we want to initialize the cbuf_meta structure and get the cbuf addr
 *
 * The return value is negative (-c) if we need to create a new level
 * in the cbuf_meta vector for c.  This is expanded by calling
 * cbuf_c_register.  We expect that we will call this function again
 * with c to get the cbuf's address via a populated cbuf_meta.
 */
int cbuf_c_create(spdid_t spdid, int size, long cbid); /* return cbid */
int cbuf_c_delete(spdid_t spdid, int cbid);
int cbuf_c_retrieve(spdid_t spdid, int cbid, int len);
vaddr_t cbuf_c_register(spdid_t spdid, long cbid);

int cbuf_c_introspect(spdid_t spdid, int iter);/* return cbid with the right order if it does exist*/
int cbuf_c_claim(spdid_t spdid, int cbid);     /* spd wants to own the cbuf, 0 means owner changes  */

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

/* #include <cbuf_vect.h> */
/* #include <mem_mgr_large.h> */

#endif 	    /* !CBUF_C_H */

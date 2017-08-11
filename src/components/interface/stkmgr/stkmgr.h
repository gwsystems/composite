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
void  stkmgr_return_stack(spdid_t s_spdid, vaddr_t addr);

#endif

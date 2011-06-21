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

#include <mem_mgr_large.h>
#include <cbuf_vect.h>

/* Component functions */
int cbuf_c_create(spdid_t spdid, int size, void *page); /* return cbid */
void cbuf_c_delete(spdid_t spdid, int cbid);
int cbuf_c_retrieve(spdid_t spdid, int cbid, int len, void *page);

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

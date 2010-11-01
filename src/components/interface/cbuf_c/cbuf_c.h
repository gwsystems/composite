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

/* Component functions */
int  cbuf_c_create(spdid_t spdid, int size, void *page); /* return cbid */
void cbuf_c_delete(spdid_t spdid, int cbid);
int  cbuf_c_retrieve(spdid_t spdid, int cbid, int len, void *page);

#endif 	    /* !CBUF_C_H */

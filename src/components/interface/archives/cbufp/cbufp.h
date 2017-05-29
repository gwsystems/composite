/**
 * Copyright 2012 by Gabriel Parmer.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gparmer@gwu.edu, 2012
 */

#ifndef   	CBUFP_H
#define   	CBUFP_H

#include <cos_component.h>

/* 
 * These are more or less identical to the counterparts in cbuf_c.h,
 * so have a look at the documentation there.
 */
int cbufp_create(spdid_t spdid, int size, long cbid);
int cbufp_delete(spdid_t spdid, int cbid);
int cbufp_retrieve(spdid_t spdid, int cbid, int len);
vaddr_t cbufp_register(spdid_t spdid, long cbid);

/* 
 * When we have no more cbufps of a specific size, lets try and
 * collect the ones we've given away.  We pass in a tmem cbuf that is
 * populated by the cbufp component with an array of cbufs that we can
 * now access.  This function returns a positive value for the number
 * of cbufs returned in the array, 0 if none are available, or a
 * negative value for an error.
 */
int cbufp_collect(spdid_t spdid, int size, long cbid_ret);

/* GAP #include <cbuf_vect.h> */
/* #include <mem_mgr_large.h> */
/* /\* Included mainly for struct cbuf_meta: *\/ */
/* #include "../cbuf_c/tmem_conf.h" */

#endif 	    /* !CBUFP_H */

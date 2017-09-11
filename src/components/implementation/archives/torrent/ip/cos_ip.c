/**
 * Copyright 2009 by Boston University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gabep1@cs.bu.edu, 2009
 */

/* 
 * This is a place-holder for when all of IP is moved here from
 * cos_net.  There should really be no performance difference between
 * having this empty like this, and having IP functionality in here.
 * If anything, due to cache (TLB) effects, having functionality in
 * here will be slower.
 */
#include <cos_component.h>
#include <torlib.h>
#include <torrent.h>
#include <cos_synchronization.h>
#include <cbuf.h>

extern td_t parent_tsplit(spdid_t spdid, td_t tid, char *param, int len, tor_flags_t tflags, long evtid);
extern void parent_trelease(spdid_t spdid, td_t tid);
extern int parent_tread(spdid_t spdid, td_t td, int cbid, int sz);
extern int parent_twrite(spdid_t spdid, td_t td, int cbid, int sz);

/* required so that we can have a rodata section */
const char *name = "cos_ip";

/* int ip_xmit(spdid_t spdid, struct cos_array *d) */
/* { */
/* 	return netif_event_xmit(cos_spd_id(), d); */
/* } */

/* int ip_wait(spdid_t spdid, struct cos_array *d) */
/* { */
/* 	return netif_event_wait(cos_spd_id(), d); */
/* } */

/* int ip_netif_release(spdid_t spdid) */
/* { */
/* 	return netif_event_release(cos_spd_id()); */
/* } */

/* int ip_netif_create(spdid_t spdid) */
/* { */
/* 	return netif_event_create(cos_spd_id()); */
/* } */

td_t 
tsplit(spdid_t spdid, td_t tid, char *param, int len, 
       tor_flags_t tflags, long evtid)
{
	td_t ret = -ENOMEM, ntd;
	struct torrent *t;

	if (tid != td_root) return -EINVAL;
	ntd = parent_tsplit(cos_spd_id(), tid, param, len, tflags, evtid);
	if (ntd <= 0) ERR_THROW(ntd, err);

	t = tor_alloc((void*)ntd, tflags);
	if (!t) ERR_THROW(-ENOMEM, err);
	ret = t->td;
err:
	return ret;
}

void
trelease(spdid_t spdid, td_t td)
{
	struct torrent *t;
	td_t ntd;

	if (!tor_is_usrdef(td)) return;
	t = tor_lookup(td);
	if (!t) goto done;
	ntd = (td_t)t->data;
	parent_trelease(cos_spd_id(), ntd);
	tor_free(t);
done:
	return;
}

int 
tmerge(spdid_t spdid, td_t td, td_t td_into, char *param, int len)
{
	return -ENOTSUP;
}

int 
twrite(spdid_t spdid, td_t td, int cbid, int sz)
{
	td_t ntd;
	struct torrent *t;
	char *buf, *nbuf;
	int ret = -1;
	cbuf_t ncbid;

	if (tor_isnull(td)) return -EINVAL;
	t = tor_lookup(td);
	if (!t) ERR_THROW(-EINVAL, done);
	if (!(t->flags & TOR_WRITE)) ERR_THROW(-EACCES, done);

	assert(t->data);
	ntd = (td_t)t->data;

	buf = cbuf2buf(cbid, sz);
	if (!buf) ERR_THROW(-EINVAL, done);

	nbuf = cbuf_alloc_ext(sz, &ncbid, CBUF_TMEM);
	assert(nbuf);
	memcpy(nbuf, buf, sz);
	ret = parent_twrite(cos_spd_id(), ntd, ncbid, sz);
	cbuf_free(ncbid);
done:
	return ret;
}

int 
tread(spdid_t spdid, td_t td, int cbid, int sz)
{
	td_t ntd;
	struct torrent *t;
	char *buf, *nbuf;
	int ret = -1;
	cbuf_t ncbid;

	if (tor_isnull(td)) return -EINVAL;
	t = tor_lookup(td);
	if (!t)                      ERR_THROW(-EINVAL, done);
	if (!(t->flags & TOR_WRITE)) ERR_THROW(-EACCES, done);

	assert(t->data);
	ntd = (td_t)t->data;

	buf = cbuf2buf(cbid, sz);
	if (!buf) ERR_THROW(-EINVAL, done);

	nbuf = cbuf_alloc_ext(sz, &ncbid, CBUF_TMEM);
	assert(nbuf);
	ret = parent_tread(cos_spd_id(), ntd, ncbid, sz);
	if (ret < 0) goto free;
	memcpy(buf, nbuf, ret);
free:
	cbuf_free(ncbid);
done:
	return ret;
}

void cos_init(void)
{
	torlib_init();
}

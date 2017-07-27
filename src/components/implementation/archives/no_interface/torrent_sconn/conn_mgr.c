/**
 * Copyright 2012 by Gabriel Parmer, gparmer@gwu.edu.  All rights
 * reserved.
 *
 * Adapted from the connection manager (no_interface/conn_mgr/) and
 * the file descriptor api (fd/api).
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#define COS_FMT_PRINT
#include <cos_component.h>
#include <cos_alloc.h>
#include <cos_debug.h>
#include <cos_list.h>
#include <cvect.h>
#include <print.h>
#include <errno.h>
#include <cos_synchronization.h>
#include <sys/socket.h>
#include <stdio.h>
#include <torrent.h>
extern td_t from_tsplit(spdid_t spdid, td_t tid, char *param, int len, tor_flags_t tflags, long evtid);
extern void from_trelease(spdid_t spdid, td_t tid);
extern int from_tread(spdid_t spdid, td_t td, int cbid, int sz);
extern int from_twrite(spdid_t spdid, td_t td, int cbid, int sz);
#include <sched.h>

static cos_lock_t sc_lock;
#define LOCK() if (lock_take(&sc_lock)) BUG();
#define UNLOCK() if (lock_release(&sc_lock)) BUG();

#define BUFF_SZ 2048//1401 //(COS_MAX_ARG_SZ/2)

CVECT_CREATE_STATIC(tor_from);
CVECT_CREATE_STATIC(tor_to);

static inline int 
tor_get_to(int from, long *teid) 
{ 
	int val = (int)cvect_lookup(&tor_from, from);
	*teid = val >> 16;
	return val & ((1<<16)-1); 
}

static inline int 
tor_get_from(int to, long *feid) 
{ 
	int val = (int)cvect_lookup(&tor_to, to);
	*feid = val >> 16;
	return val & ((1<<16)-1); 
}

static inline void 
tor_add_pair(int from, int to, long feid, long teid)
{
#define MAXVAL (1<<16)
	assert(from < MAXVAL);
	assert(to   < MAXVAL);
	assert(feid < MAXVAL);
	assert(teid < MAXVAL);
	if (cvect_add(&tor_from, (void*)((teid << 16) | to), from) < 0) BUG();
	if (cvect_add(&tor_to, (void*)((feid << 16) | from), to) < 0) BUG();
}

static inline void
tor_del_pair(int from, int to)
{
	cvect_del(&tor_from, from);
	cvect_del(&tor_to, to);
}

CVECT_CREATE_STATIC(evts);
#define EVT_CACHE_SZ 1
int evt_cache[EVT_CACHE_SZ];
int ncached = 0;

long evt_all = 0;

static inline long
evt_wait_all(void) { return evt_wait(cos_spd_id(), evt_all); }

/* 
 * tor > 0 == event is "from"
 * tor < 0 == event is "to"
 */
static inline long
evt_get(void)
{
	long eid;

	if (!evt_all) evt_all = evt_split(cos_spd_id(), 0, 1);
	assert(evt_all);

	eid = (ncached == 0) ?
		evt_split(cos_spd_id(), evt_all, 0) :
		evt_cache[--ncached];
	assert(eid > 0);

	return eid;
}

static inline void
evt_put(long evtid)
{
	if (ncached >= EVT_CACHE_SZ) evt_free(cos_spd_id(), evtid);
	else                         evt_cache[ncached++] = evtid;
}

/* positive return value == "from", negative == "to" */
static inline int
evt_torrent(long evtid) { return (int)cvect_lookup(&evts, evtid); }

static inline void
evt_add(int tid, long evtid) { cvect_add(&evts, (void*)tid, evtid); }

struct tor_conn {
	int  from, to;
	long feid, teid;
};

static inline void 
mapping_add(int from, int to, long feid, long teid)
{
	long tf, tt;

	LOCK();
	tor_add_pair(from, to, feid, teid);
	evt_add(from,    feid);
	evt_add(to * -1, teid);
	assert(tor_get_to(from, &tt) == to);
	assert(tor_get_from(to, &tf) == from);
	assert(evt_torrent(feid) == from);
	assert(evt_torrent(teid) == (-1*to));
	assert(tt == teid);
	assert(tf == feid);
	UNLOCK();
}

static inline void
mapping_remove(int from, int to, long feid, long teid)
{
	LOCK();
	tor_del_pair(from, to);
	cvect_del(&evts, feid);
	cvect_del(&evts, teid);
	UNLOCK();
}

static void 
accept_new(int accept_fd)
{
	int from, to, feid, teid;

	while (1) {
		feid = evt_get();
		assert(feid > 0);
		from = from_tsplit(cos_spd_id(), accept_fd, "", 0, TOR_RW, feid);
		assert(from != accept_fd);
		if (-EAGAIN == from) {
			evt_put(feid);
			return;
		} else if (from < 0) {
			printc("from torrent returned %d\n", from);
			BUG();
			return;
		}
		teid = evt_get();
		assert(teid > 0);
		to = tsplit(cos_spd_id(), td_root, "", 0, TOR_RW, teid);
		if (to < 0) {
			printc("torrent split returned %d", to);
			BUG();
		}

		mapping_add(from, to, feid, teid);
	}
}

static void 
from_data_new(struct tor_conn *tc)
{
	int from, to, amnt;
	char *buf;
	cbuf_t cb;

	from = tc->from;
	to   = tc->to;
	while (1) {
		int ret;

		buf = cbuf_alloc_ext(BUFF_SZ, &cb, CBUF_TMEM);
		assert(buf);
		amnt = from_tread(cos_spd_id(), from, cb, BUFF_SZ-1);
		if (0 == amnt) break;
		else if (-EPIPE == amnt) {
			goto close;
		} else if (amnt < 0) {
			printc("read from fd %d produced %d.\n", from, amnt);
			BUG();
		}
		assert(amnt <= BUFF_SZ);
		if (amnt != (ret = twrite(cos_spd_id(), to, cb, amnt))) {
			printc("conn_mgr: write failed w/ %d on fd %d\n", ret, to);
			goto close;

		}
		cbuf_free(cb);
	}
done:
	cbuf_free(cb);
	return;
close:
	mapping_remove(from, to, tc->feid, tc->teid);
	from_trelease(cos_spd_id(), from);
	trelease(cos_spd_id(), to);
	assert(tc->feid && tc->teid);
	evt_put(tc->feid);
	evt_put(tc->teid);
	goto done;
}

static void 
to_data_new(struct tor_conn *tc)
{
	int from, to, amnt;
	char *buf;
	cbuf_t cb;

	from = tc->from;
	to   = tc->to;
	while (1) {
		int ret;

		if (!(buf = cbuf_alloc_ext(BUFF_SZ, &cb, CBUF_TMEM))) BUG();
		amnt = tread(cos_spd_id(), to, cb, BUFF_SZ-1);
		if (0 == amnt) break;
		else if (-EPIPE == amnt) {
			goto close;
		} else if (amnt < 0) {
			printc("read from fd %d produced %d.\n", from, amnt);
			BUG();
		}
		assert(amnt <= BUFF_SZ);
		if (amnt != (ret = from_twrite(cos_spd_id(), from, cb, amnt))) {
			printc("conn_mgr: write failed w/ %d of %d on fd %d\n", 
			       ret, amnt, to);
			goto close;
		}
		cbuf_free(cb);
	}
done:
	cbuf_free(cb);
	return;
close:
	mapping_remove(from, to, tc->feid, tc->teid);
	from_trelease(cos_spd_id(), from);
	trelease(cos_spd_id(), to);
	assert(tc->feid && tc->teid);
	evt_put(tc->feid);
	evt_put(tc->teid);
	goto done;
}

void
cos_init(void *arg)
{
	int c, accept_fd, ret;
	long eid;
	char *init_str = cos_init_args(), *create_str;
	int lag, nthds, prio;
	
	cvect_init_static(&evts);
	cvect_init_static(&tor_from);
	cvect_init_static(&tor_to);
	lock_static_init(&sc_lock);
		
	sscanf(init_str, "%d:%d:%d", &lag, &nthds, &prio);
	printc("lag: %d, nthds:%d, prio:%d\n", lag, nthds, prio);
	create_str = strstr(init_str, "/");
	assert(create_str);

	eid = evt_get();
	ret = c = from_tsplit(cos_spd_id(), td_root, create_str, strlen(create_str), TOR_ALL, eid);
	if (ret <= td_root) BUG();
	accept_fd = c;
	evt_add(c, eid);

	/* event loop... */
	while (1) {
		struct tor_conn tc;
		int t;
		long evt;

		memset(&tc, 0, sizeof(struct tor_conn));
		evt = evt_wait_all();
		t   = evt_torrent(evt);

		if (t > 0) {
			tc.feid = evt;
			tc.from = t;
			if (t == accept_fd) {
				tc.to = 0;
				accept_new(accept_fd);
			} else {
				tc.to = tor_get_to(t, &tc.teid);
				assert(tc.to > 0);
				from_data_new(&tc);
			}
		} else {
			t *= -1;
			tc.teid = evt;
			tc.to   = t;
			tc.from = tor_get_from(t, &tc.feid);
			assert(tc.from > 0);
			to_data_new(&tc);
		}
	}
}

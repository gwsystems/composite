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
#include <cos_net.h>
#include <net_transport.h>

#include <torrent.h>
#include <sched.h>

#define BUFF_SZ 1401 //(COS_MAX_ARG_SZ/2)

CVECT_CREATE_STATIC(tor_from);
CVECT_CREATE_STATIC(tor_to);

static inline int 
tor_get_to(int from) { return (int)cvect_lookup(&tor_from, from); }

static inline int 
tor_get_from(int to) { return (int)cvect_lookup(&tor_to, to); }

static inline void 
tor_add_pair(int from, int to)
{
	if (cvect_add(&tor_from, (void*)to, from) < 0) BUG();
	if (cvect_add(&tor_to, (void*)from, to) < 0) BUG();
}

static inline void
tor_del_pair(int from, int to)
{
	cvect_del(&tor_from, from);
	cvect_del(&tor_to, to);
}

CVECT_CREATE_STATIC(evts);
#define EVT_CACHE_SZ 128
int evt_cache[EVT_CACHE_SZ];
int ncached = 0;

/* 
 * tor > 0 == event is "from"
 * tor < 0 == event is "to"
 */
static inline long
evt_get(void)
{
	long eid = (ncached == 0) ?
		evt_create(cos_spd_id()) :
		evt_cache[--ncached];
	assert(eid > 0);

	return eid;
}

static inline void
evt_put(long evtid)
{
	if (ncached >= EVT_CACHE_SZ) evt_free(cos_spd_id(), evtid);
	else                         evt_cache[ncached++] = evtid;
	cvect_del(&evts, evtid);
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
	tor_add_pair(from, to);
	evt_add(from,    feid);
	evt_add(to * -1, teid);
	assert(tor_get_to(from) == to);
	assert(tor_get_from(to) == from);
	assert(evt_torrent(feid) == from);
	assert(evt_torrent(teid) == (-1*to));
}

static void accept_new(int accept_fd)
{
	int from, to, feid, teid;

	while (1) {
		from = net_accept(cos_spd_id(), accept_fd);
		assert(from != accept_fd);
		if (-EAGAIN == from) {
			return;
		} else if (from < 0) {
			BUG();
			return;
		}
		feid = evt_get();
		assert(feid > 0);
		if (0 < net_accept_data(cos_spd_id(), from, feid)) BUG();

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

static void from_data_new(struct tor_conn *tc)
{
	int from, to, amnt;
	char *buf;

	from = tc->from;
	to   = tc->to;
	buf = cos_argreg_alloc(BUFF_SZ);
	assert(buf);
	while (1) {
		int ret;

		amnt = net_recv(cos_spd_id(), from, buf, BUFF_SZ-1);
		if (0 == amnt) break;
		else if (-EPIPE == amnt) {
			goto close;
		} else if (amnt < 0) {
			printc("read from fd %d produced %d.\n", from, amnt);
			BUG();
		}
		if (amnt != (ret = twrite_pack(cos_spd_id(), to, buf, amnt))) {
			printc("conn_mgr: write failed w/ %d on fd %d\n", ret, to);
			goto close;

		}
	}
done:
	cos_argreg_free(buf);
	return;
close:
	net_close(cos_spd_id(), from);
	trelease(cos_spd_id(), to);
	tor_del_pair(from, to);
	if (tc->feid) cvect_del(&evts, tc->feid);
	if (tc->teid) cvect_del(&evts, tc->teid);
	goto done;
}

static void to_data_new(struct tor_conn *tc)
{
	int from, to, amnt;
	char *buf;

	from = tc->from;
	to   = tc->to;
	buf = cos_argreg_alloc(BUFF_SZ);
	assert(buf);
	while (1) {
		int ret;

		amnt = tread_pack(cos_spd_id(), to, buf, BUFF_SZ-1);
		if (0 == amnt) break;
		else if (-EPIPE == amnt) {
			goto close;
		} else if (amnt < 0) {
			printc("read from fd %d produced %d.\n", from, amnt);
			BUG();
		}
		if (amnt != (ret = net_send(cos_spd_id(), from, buf, amnt))) {
			printc("conn_mgr: write failed w/ %d on fd %d\n", ret, to);
			goto close;

		}

	}
done:
	cos_argreg_free(buf);
	return;
close:
	net_close(cos_spd_id(), from);
	trelease(cos_spd_id(), to);
	tor_del_pair(from, to);
	if (tc->feid) cvect_del(&evts, tc->feid);
	if (tc->teid) cvect_del(&evts, tc->teid);
	goto done;
}

void cos_init(void *arg)
{
	int c, accept_fd, ret;
	long eid;

	cvect_init_static(&evts);
	cvect_init_static(&tor_from);
	cvect_init_static(&tor_to);
	
	eid = evt_get();
	c = net_create_tcp_connection(cos_spd_id(), cos_get_thd_id(), eid);
	if (c < 0) BUG();
	ret = net_bind(cos_spd_id(), c, 0, 200);
	if (ret < 0) BUG();
	ret = net_listen(cos_spd_id(), c, 255);
	if (ret < 0) BUG();
	accept_fd = c;
	evt_add(c, eid);

	while (1) {
		struct tor_conn tc;
		int t;
		long evt;

		memset(&tc, 0, sizeof(struct tor_conn));
		printc("waiting...\n");
		evt = evt_grp_wait(cos_spd_id());
		t   = evt_torrent(evt);

		if (t > 0) {
			tc.feid = evt;
			tc.from = t;
			if (t == accept_fd) {
				tc.to = 0;
				printc("accepting event.\n");
				accept_new(accept_fd);
			} else {
				tc.to = tor_get_to(t);
				assert(tc.to > 0);
				printc("data from net.\n");
				from_data_new(&tc);
			}
		} else {
			t *= -1;
			tc.teid = evt;
			tc.to   = t;
			tc.from = tor_get_from(t);
			assert(tc.from > 0);
			printc("data from torrent.\n");
			to_data_new(&tc);
		}

		cos_mpd_update();
	}
}

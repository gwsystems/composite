/**
 * Copyright 2012 by Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <cos_component.h>
#include <torrent.h>
#include <torlib.h>

#include <cbuf.h>
#include <print.h>
#include <cos_synchronization.h>
#include <evt.h>
#include <cos_alloc.h>
#include <cos_map.h>
#include <cringbuf.h>
#include <ck_ring.h>
#define MBOX_BUFFER_SIZE 256
struct cbuf_info {
	cbuf_t cb;
	int sz, off;
};
CK_RING_PROTOTYPE(cb_buffer, cbuf_info);
enum {
	SERV   = 0,
	CLIENT = 1
};

struct as_conn {
	spdid_t owner;
	int     status;
	struct torrent *ts[2];
	struct ck_ring cbs[2];
	struct cbuf_info *buffer[2];
	struct as_conn *next, *prev;
};

struct as_conn_root {
	struct torrent *t; /* server evtid */
	spdid_t owner;
	struct as_conn cs; /* client initiated, but not yet accepted connections */
};

static void
__mbox_remove_cbufp(struct as_conn *ac)
{
	int i;

	for (i = 0 ; i < 2 ; i++) {
		struct cbuf_info cbi;
		void *addr;

		while (CK_RING_DEQUEUE_SPSC(cb_buffer, &ac->cbs[i], &cbi)) {
			addr = cbuf2buf(cbi.cb, cbi.sz);
			assert(addr);
			cbuf_free(cbi.cb);
		}
	}

}

static void
free_as_conn_root(void *d)
{
	struct as_conn_root *acr = d;
	struct as_conn *i;

	for (i = FIRST_LIST(&acr->cs, next, prev) ;
	     i != &acr->cs ;
	     i = FIRST_LIST(i, next, prev)) {
		i->status = -EPIPE;
		__mbox_remove_cbufp(i);
		evt_trigger(cos_spd_id(), i->ts[CLIENT]->evtid);
	}
	for (i = FIRST_LIST(&acr->cs, next, prev) ;
	     i != &acr->cs ;
	     i = FIRST_LIST(&acr->cs, next, prev)) {
		REM_LIST(i, next, prev);
	}
	free(acr);
}
#define FS_DATA_FREE free_as_conn_root
#include <fs.h>

static cos_lock_t fs_lock;
struct fsobj root;
#define LOCK() if (lock_take(&fs_lock)) BUG();
#define UNLOCK() if (lock_release(&fs_lock)) BUG();

#define MAX_ALLOC_SZ 4096
#define MAX_DATA_SZ (MAX_ALLOC_SZ - sizeof(struct __cringbuf))

static struct fsobj *
mbox_create_addr(spdid_t spdid, struct torrent *t, struct fsobj *parent,
		 char *subpath, tor_flags_t tflags, int *_ret)
{
	int ret = 0;
	struct fsobj *fsc = NULL;;
	struct as_conn_root *acr;

	assert(parent);
	if (!(parent->flags & TOR_SPLIT)) ERR_THROW(-EACCES, done);
	fsc = fsobj_alloc(subpath, parent);
	if (!fsc) ERR_THROW(-EINVAL, done);
	fsc->flags    = tflags;

	acr = malloc(sizeof(struct as_conn_root));
	if (!acr) ERR_THROW(-ENOMEM, free);
	acr->owner    = spdid;
	acr->t = t;
	INIT_LIST(&acr->cs, next, prev);
	fsc->data     = (void*)acr;

	fsc->allocated = fsc->size = 0;
done:
	*_ret = ret;
	return fsc;
free:
	fsobj_release(fsc);
	fsc = NULL;
	goto done;
}

/*
 * Create an end-point for this specific mail-box.
 */
static int
mbox_create_server(struct torrent *t, struct as_conn_root *acr)
{
	int ret = 0;
	struct as_conn *ac;
	assert(!t->data);

	if (EMPTY_LIST(&acr->cs, next, prev)) return -EAGAIN;
	ac = FIRST_LIST(&acr->cs, next, prev);
	REM_LIST(&acr->cs, next, prev);

	ac->ts[SERV] = t;
	t->data = ac;
	assert(ac->ts[CLIENT]);
	evt_trigger(cos_spd_id(), ac->ts[CLIENT]->evtid);

	return ret;
}

static int
mbox_create_client(struct torrent *t, struct as_conn_root *acr)
{
	struct as_conn *ac;
	int i, ret = 0;
	assert(!t->data);

	ac = malloc(sizeof(struct as_conn));
	if (!ac) return -ENOMEM;
	ac->status = 0;
	ac->owner  = acr->owner;
	ac->ts[CLIENT] = t;
	t->data = ac;
	for (i = 0 ; i < 2 ; i++) {
		ac->buffer[i] = alloc_page();
		ck_ring_init(&ac->cbs[i], leqpow2(PAGE_SIZE / sizeof(struct cbuf_info)));

	}
	ADD_END_LIST(&acr->cs, ac, next, prev);
	if (acr->t) evt_trigger(cos_spd_id(), acr->t->evtid);
	return ret;
}

static int
mbox_put(struct torrent *t, cbuf_t cb, int sz, int off, int ep)
{
	struct as_conn *ac;
	int other_ep = !ep;
	int ret = 0;
	struct cbuf_info cbi;

	if (sz < 1) return -EAGAIN;
	ac  = t->data;
	if (ac->status) return ac->status;
	cbi.cb  = cb;
	cbi.sz  = sz;
	cbi.off = off;
	ret = CK_RING_ENQUEUE_SPSC(cb_buffer, &ac->cbs[ep], ac->buffer[ep], &cbi);
	if (ret == 0) return -EALREADY;
	evt_trigger(cos_spd_id(), ac->ts[other_ep]->evtid);
	return ret;
}

static int
mbox_get(struct torrent *t, int *sz, int *off, int ep)
{
	struct as_conn *ac;
	struct cbuf_info cbi;
	int other_ep = !ep;
	cbuf_t cb;

	ac  = t->data;
	if (!CK_RING_DEQUEUE_SPSC(cb_buffer, &ac->cbs[other_ep], ac->buffer[ep], &cbi)) {
		if (ac->status) return ac->status;
		return -EAGAIN;
	}
	cb   = cbi.cb;
	*off = cbi.off;
	*sz  = cbi.sz;
	return cb;
}

/*
 * Protocol for use of this component:
 *
 * 1) server of data, c_s issues a tsplit with the identifying string,
 * s (set with param, and any torrent we're split off of) -> td_s
 *
 * 2) c_s issues a split on s.  This split create the evtid that can
 * be later waited on, and will complete later when a request comes in
 * from the client (see NOTE)
 *
 * 3) a client, c_c issues a tsplit on s -> td_c.  This torrent can
 * later be split with param == "", and it will create the new
 * connection.
 *
 * NOTE: at any point, split will return -EAGAIN until for the server
 * until a client has arrived.
 *
 * 4) c_s receives event notification that a split is available on
 * td_s.  When split with td = td_s -> td_s^new
 *
 * 5) at this point, anything read or written to td_s^new or td_c will
 * be asynchronously written to the other.  When either side releases
 * the descriptor, -EPIPE will be sent to the other
 */

td_t
tsplit(spdid_t spdid, td_t td, char *param,
       int len, tor_flags_t tflags, long evtid)
{
	td_t ret = -1;
	struct torrent *t, *nt;
	struct fsobj *fsc, *parent = NULL; /* child, and parent */
	char *subpath;

	LOCK();
	t = tor_lookup(td);
	if (!t) ERR_THROW(-EINVAL, done);

	nt = tor_alloc(NULL, tflags);
	if (!nt) ERR_THROW(-ENOMEM, done);
	nt->evtid = evtid;

	fsc = fsobj_path2obj(param, len, t->data, &parent, &subpath);
	if (!fsc) {
		if (!(tflags & TOR_NONPERSIST)) ERR_THROW(-EINVAL, free);
		fsc = mbox_create_addr(spdid,nt, parent, subpath, tflags, (int*)&ret);
		if (!fsc) goto free; /* ret set above... */
		nt->data = fsc;
		nt->flags = tflags & TOR_SPLIT;
	} else {
		struct as_conn_root *acr = (struct as_conn_root*)fsc->data;
		assert(acr);
	        if ((~fsc->flags) & tflags) ERR_THROW(-EACCES, free);
		/* Case 2: a client attempt to connect to a server*/
	        if (len>0)      ret = (td_t)mbox_create_client(nt, acr);
		/* Case 3: a server create a connection to a client*/
	        else            ret = (td_t)mbox_create_server(nt, acr);
	        if (ret < 0) goto free;
	        nt->flags = tflags & TOR_RW;
	}
	ret = nt->td;
done:
	UNLOCK();
	return ret;
free:
	tor_free(nt);
	goto done;
}

int
tmerge(spdid_t spdid, td_t td, td_t td_into, char *param, int len)
{
	return -ENOTSUP;
}
int
twrite(spdid_t spdid, td_t td, int cbid, int sz)
{
	return -ENOTSUP;
}
void
trelease(spdid_t spdid, td_t td)
{
	struct torrent *t;

	if (!tor_is_usrdef(td)) return;

	LOCK();
	t = tor_lookup(td);
	if (!t) goto done;

	/* TODO: add TOR_NONPERSIST: on release, remove from the
	 * namespace (if no existing references to the resource) */

	if (t->flags & TOR_SPLIT) {
		struct fsobj *fsc;
		fsc = (struct fsobj *)t->data;
		fsobj_rem(fsc, fsc->parent);
		fsobj_release(fsc);
	} else {
		struct as_conn *ac = t->data;
		int other = 1; 	/* does the other torrent exist? */

		/* FIXME: add calls to __mbox_remove_cbufp */
		ac->status = -EPIPE;
		if (ac->ts[0] == t) {
			ac->ts[0] = NULL;
			if (!ac->ts[1]) other = 0;
			else            evt_trigger(cos_spd_id(), ac->ts[1]->evtid);
		} else if (ac->ts[1] == t) {
			ac->ts[1] = NULL;
			if (!ac->ts[0]) other = 0;
			else            evt_trigger(cos_spd_id(), ac->ts[0]->evtid);
		} else {
			assert(0);
		}
		/* no torrents are accessing the as connection...free it */
		if (!other) {
			/*struct cringbuf *rb;

			  rb = &ac->rbs[0];
			  free(rb->b);
			  rb = &ac->rbs[1];
			  free(rb->b);*/
			free(ac->buffer[0]);
			free(ac->buffer[1]);
			free(ac);
		}
	}
	tor_free(t);
done:
	UNLOCK();
	return;
}

int
tread(spdid_t spdid, td_t td, int cbid, int sz)
{
	return -ENOTSUP;
}

int
treadp(spdid_t spdid, td_t td, int *off, int *sz)
{
	cbuf_t ret;
	struct torrent *t;
	struct as_conn *ac;

	if (tor_isnull(td)) return -EINVAL;

	LOCK();
	t = tor_lookup(td);
	if (!t) ERR_THROW(-EINVAL, done);
	assert(!tor_is_usrdef(td) || t->data);
	if (!(t->flags & TOR_READ)) ERR_THROW(-EACCES, done);

	ac = t->data;
	ret = mbox_get(t, sz, off, ac->owner != spdid);
	if ((int)ret < 0) goto done;
	t->offset += ret;
done:
	UNLOCK();
	return ret;
}

int
twritep(spdid_t spdid, td_t td, int cbid, int sz)
{
	int ret = -1;
	struct torrent *t;
	struct as_conn *ac;
	cbuf_t cb = cbid;

	if (tor_isnull(td)) return -EINVAL;
	LOCK();
	t = tor_lookup(td);
	if (!t) ERR_THROW(-EINVAL, done);
	assert(t->data);
	if (!(t->flags & TOR_WRITE)) ERR_THROW(-EACCES, done);
	ac = t->data;
	ret = mbox_put(t, cb, sz, 0, ac->owner != spdid);
	if (ret < 0) goto done;
	t->offset += ret;
done:
	UNLOCK();
	return ret;
}

int cos_init(void)
{
	lock_static_init(&fs_lock);
	torlib_init();

	fs_init_root(&root);
	root_torrent.data = &root;
	root.flags = TOR_READ | TOR_SPLIT;

	return 0;
}

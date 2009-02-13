/**
 * Copyright 2008 by Gabriel Parmer, gabep1@cs.bu.edu.  All rights
 * reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

/* 
 * NOT COMPLETE AND NOT TESTED...
 */
#define COS_FMT_PRINT

#include <cos_component.h>
#include <cos_net.h>
#include <cos_synchronization.h>

#include <errno.h>

extern int sched_block(spdid_t spdid);

/* event functions */
extern int evt_create(spdid_t spdid, long extern_evt);
extern void evt_free(spdid_t spdid, long extern_evt);
extern int evt_wait(spdid_t spdid, long extern_evt);
extern long evt_grp_wait(spdid_t spdid);
extern int evt_trigger(spdid_t spdid, long extern_evt);

/* network functions */
extern int net_send(spdid_t spdid, net_connection_t nc, void *data, int sz);
extern int net_recv(spdid_t spdid, net_connection_t nc, void *data, int sz);

#define MAX_FDS 512

typedef enum {
	DESC_FREE,
	DESC_TOP,
	DESC_NET
} desc_t;

struct descriptor {
	desc_t type;
	void *data;
	struct descriptor *free;
};

struct descriptor fds[MAX_FDS];
struct descriptor *freelist;
cos_lock_t fd_lock;
#define FD_LOCK_TAKE() 	lock_take(&fd_lock)
#define FD_LOCK_RELEASE() lock_release(&fd_lock)

static inline int fd_get_index(struct descriptor *d)
{
	return d-&fds[0];
}

static inline struct descriptor *fd_get_desc(int fd)
{
	struct descriptor *d;

	if (fd >= MAX_FDS) return NULL;
	d = &fds[fd];
	if (DESC_FREE == d->type) return NULL;
	return d;
}

static struct descriptor *fd_alloc(desc_t t)
{
	struct descriptor *d;

	d = freelist;
	if (NULL != d) {
		freelist = freelist->free;
		d->free = NULL;
		assert(DESC_FREE == d->type);
		d->type = t;
		return d;
	}
	return NULL;
}

static void fd_free(struct descriptor *d)
{
	d->type = DESC_FREE;
	d->data = NULL;
	d->free = freelist;
	freelist = d;
}

/* 
 * Network specific functions
 * FIXME: move into a socket api component
 */
#include <sys/types.h>
#include <sys/socket.h>

/* Network functions */
extern net_connection_t net_create_tcp_connection(spdid_t spdid, u16_t tid, long data);
extern net_connection_t net_create_udp_connection(spdid_t spdid, long data);
extern net_connection_t net_accept(spdid_t spdid, net_connection_t nc);
extern int net_accept_data(spdid_t spdid, net_connection_t nc, long data);
extern int net_listen(spdid_t spdid, net_connection_t nc);
extern int net_bind(spdid_t spdid, net_connection_t nc, u32_t ip, u16_t port);
extern int net_connect(spdid_t spdid, net_connection_t nc, u32_t ip, u16_t port);
extern int net_close(spdid_t spdid, net_connection_t nc);

int cos_socket(int domain, int type, int protocol)
{
	net_connection_t nc;
	struct descriptor *d;
	int fd;

	if (PF_INET != domain) return -1;
	if (0 != protocol)     return -1;

	FD_LOCK_TAKE();
	if (NULL == (d = fd_alloc(DESC_NET))) goto err;
	fd = fd_get_index(d);
	if (evt_create(cos_spd_id(), fd)) assert(0);

	switch (type) {
	case SOCK_STREAM:
		nc = net_create_tcp_connection(cos_spd_id(), cos_get_thd_id(), fd);
		break;
	case SOCK_DGRAM:
		nc = net_create_udp_connection(cos_spd_id(), fd);
		break;
	default:
		goto err_cleanup;
	}
	if (nc < 0) {
		goto err_cleanup;
	}
	d->data = (void *)nc;
	FD_LOCK_RELEASE();

	return fd;
err_cleanup:
	fd_free(d);
	evt_free(cos_spd_id(), fd);
	/* fall through */
err:
	FD_LOCK_RELEASE();
	return -1;
}

int cos_listen(int fd)
{
	struct descriptor *d;
	net_connection_t nc;
	int ret;

	FD_LOCK_TAKE();
	d = fd_get_desc(fd);
	if (NULL == d) goto err;
	if (d->type != DESC_NET) goto err;
	nc = (net_connection_t)d->data;
	FD_LOCK_RELEASE();
	ret = net_listen(cos_spd_id(), nc);
	return ret;
err:
	FD_LOCK_RELEASE();
	return -1;
}

int cos_bind(int fd, u32_t ip, u16_t port)
{
	struct descriptor *d;
	net_connection_t nc;

	FD_LOCK_TAKE();
	d = fd_get_desc(fd);
	if (NULL == d) goto err;
	if (d->type != DESC_NET) goto err;
	nc = (net_connection_t)d->data;
	FD_LOCK_RELEASE();
	return net_bind(cos_spd_id(), nc, ip, port);
err:
	FD_LOCK_RELEASE();
	return -1;
}

int cos_accept(int fd)
{
	struct descriptor *d, *d_new;
	net_connection_t nc_new, nc;

	do {
		FD_LOCK_TAKE();
		d = fd_get_desc(fd);
		if (NULL == d) goto err;
		if (d->type != DESC_NET) goto err;
		nc = (net_connection_t)d->data;
		FD_LOCK_RELEASE();
		nc_new = net_accept(cos_spd_id(), nc);
		if (-EAGAIN == nc_new) {
			if (evt_wait(cos_spd_id(), fd_get_index(d))) assert(0);
		} else if (nc_new < 0) 
			return -1;
	} while (-EAGAIN == nc_new);

	FD_LOCK_TAKE();
	/* If this error is triggered, we should also close the nc */
	if (NULL == (d_new = fd_alloc(DESC_NET))) {
		FD_LOCK_RELEASE();
		net_close(cos_spd_id(), nc);
		return -1;
	}
	fd = fd_get_index(d_new);
	d_new->data = (void*)nc_new;
	if (evt_create(cos_spd_id(), fd)) assert(0);
	if (0 < net_accept_data(cos_spd_id(), nc_new, fd)) assert(0);
	FD_LOCK_RELEASE();

	return fd_get_index(d_new);
err:
	FD_LOCK_RELEASE();
	return -1;
}

int cos_close(int fd)
{
	struct descriptor *d;
	net_connection_t nc;

	FD_LOCK_TAKE();
	d = fd_get_desc(fd);
	if (NULL == d) goto err;
	if (d->type != DESC_NET) goto err;
	nc = (net_connection_t)d->data;
	fd_free(d);
	FD_LOCK_RELEASE();
	evt_free(cos_spd_id(), fd);
	return net_close(cos_spd_id(), nc);
err:
	FD_LOCK_RELEASE();
	return -1;
}

int cos_read(int fd, char *buf, int sz)
{
	struct descriptor *d;
	net_connection_t nc;
	int ret = -1;

	if (!cos_argreg_buff_intern(buf, sz)) return -EFAULT;
	FD_LOCK_TAKE();
	d = fd_get_desc(fd);
	if (NULL == d) goto err;
	if (d->type != DESC_NET) goto err;
	nc = (net_connection_t)d->data;
	FD_LOCK_RELEASE();
	ret = net_recv(cos_spd_id(), nc, buf, sz);
	if (unlikely(0 > ret)) {
		FD_LOCK_TAKE();
		d = fd_get_desc(fd);
		if (NULL != d && (net_connection_t)d->data == nc) {
			fd_free(d);
			evt_free(cos_spd_id(), fd);
			net_close(cos_spd_id(), nc);
		}
		ret = -EPIPE;
		FD_LOCK_RELEASE();
	}
	return ret;
err:
	FD_LOCK_RELEASE();
	return -1;
}

int cos_write(int fd, char *buf, int sz)
{
	struct descriptor *d;
	net_connection_t nc;
	int ret = -1;

	if (!cos_argreg_buff_intern(buf, sz)) return -EFAULT;
	FD_LOCK_TAKE();
	d = fd_get_desc(fd);
	if (NULL == d) goto err;
	if (d->type != DESC_NET) goto err;
	nc = (net_connection_t)d->data;
	FD_LOCK_RELEASE();
	ret = net_send(cos_spd_id(), nc, buf, sz);
	if (unlikely(0 > ret)) {
		FD_LOCK_TAKE();
		d = fd_get_desc(fd);
		if (NULL != d && (net_connection_t)d->data == nc) {
			fd_free(d);
			evt_free(cos_spd_id(), fd);
			net_close(cos_spd_id(), nc);
		}
		ret = -EPIPE;
		FD_LOCK_RELEASE();
	}
	return ret;
err:
	FD_LOCK_RELEASE();
	return -1;
}

int cos_wait(int fd)
{
	return evt_wait(cos_spd_id(), fd);
}

int cos_wait_all(void)
{
	return (int)evt_grp_wait(cos_spd_id());
}

static void init(void) 
{
	int i;

	lock_static_init(&fd_lock);
	freelist = &fds[0];
	for (i = 0 ; i < MAX_FDS ; i++) {
		fds[i].type = DESC_FREE;
		fds[i].data = NULL;
		fds[i].free = (i < MAX_FDS-1) ? &fds[i+1] : NULL;
	}

	sched_block(cos_spd_id());
}

void cos_init(void *arg)
{
	volatile static int first = 1;

	if (first) {
		first = 0;
		init();
		assert(0);
	} else {
		prints("fd: not expecting more than one bootstrap.");
	}
}

void bin(void)
{
	sched_block(cos_spd_id());
}

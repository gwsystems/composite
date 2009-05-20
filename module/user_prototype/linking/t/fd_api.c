/**
 * Copyright 2008 by Boston University.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author:  Gabriel Parmer, gabep1@cs.bu.edu, 2009
 */

#define COS_FMT_PRINT

#include <cos_component.h>
#include <cos_net.h>
#include <cos_synchronization.h>

#include <errno.h>

#include <cos_alloc.h>
#include <cos_map.h>

extern int sched_block(spdid_t spdid);

/* event functions */
extern long evt_create(spdid_t spdid);
extern void evt_free(spdid_t spdid, long extern_evt);
extern int evt_wait(spdid_t spdid, long extern_evt);
extern long evt_grp_wait(spdid_t spdid);
extern int evt_trigger(spdid_t spdid, long extern_evt);
extern int evt_set_prio(spdid_t spdid, long extern_evt, int prio);

/* network functions */
extern int net_send(spdid_t spdid, net_connection_t nc, void *data, int sz);
extern int net_recv(spdid_t spdid, net_connection_t nc, void *data, int sz);

typedef enum {
	DESC_TOP,
	DESC_NET,
	DESC_HTTP
} desc_t;

struct descriptor;
struct fd_ops {
	int (*close)(int fd, struct descriptor *d);
	int (*read)(int fd, struct descriptor *d, char *buff, int sz);
	int (*write)(int fd, struct descriptor *d, char *buff, int sz);
};

struct descriptor {
	int fd_num;
	long evt_id;
	desc_t type;
	void *data;
	struct descriptor *free;

	struct fd_ops ops;
};

COS_VECT_CREATE_STATIC(evt2fdesc);
COS_MAP_CREATE_STATIC(fds);
cos_lock_t fd_lock;
#define FD_LOCK_TAKE() 	lock_take(&fd_lock)
#define FD_LOCK_RELEASE() lock_release(&fd_lock)

static struct descriptor *evt2fd_lookup(long evt_id)
{
	return cos_vect_lookup(&evt2fdesc, evt_id);
}

static void evt2fd_create(long evt_id, struct descriptor *d)
{
	assert(NULL == evt2fd_lookup(evt_id));
	if (0 > cos_vect_add_id(&evt2fdesc, d, evt_id)) assert(0);
}

static void evt2fd_remove(long evt_id)
{
	cos_vect_del(&evt2fdesc, evt_id);
}

static inline int fd_get_index(struct descriptor *d)
{
	return d->fd_num;
}

static inline struct descriptor *fd_get_desc(int fd)
{
	return cos_map_lookup(&fds, fd);
}

static struct descriptor *fd_alloc(desc_t t)
{
	struct descriptor *d;
	int id;

	d = malloc(sizeof(struct descriptor));
	if (NULL == d) return NULL;
	d->type = t;
	id = cos_map_add(&fds, d);
	if (-1 == id) {
		free(d);
		return NULL;
	}
	d->fd_num = (int)id;

	return d;
}

static void fd_free(struct descriptor *d)
{
	cos_map_del(&fds, (long)d->fd_num);
	evt2fd_remove(d->evt_id);
	free(d);
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
extern int net_listen(spdid_t spdid, net_connection_t nc, int queue_len);
extern int net_bind(spdid_t spdid, net_connection_t nc, u32_t ip, u16_t port);
extern int net_connect(spdid_t spdid, net_connection_t nc, u32_t ip, u16_t port);
extern int net_close(spdid_t spdid, net_connection_t nc);

static int fd_net_close(int fd, struct descriptor *d)
{
	net_connection_t nc;
	int ret;

	assert(d->type == DESC_NET)
	nc = (net_connection_t)d->data;
	ret = net_close(cos_spd_id(), nc);
	evt_free(cos_spd_id(), d->evt_id);
	fd_free(d);
	FD_LOCK_RELEASE();

	return ret;
}

static int fd_net_read(int fd, struct descriptor *d, char *buf, int sz)
{
	net_connection_t nc;

	assert(d->type == DESC_NET);
	nc = (net_connection_t)d->data;
	FD_LOCK_RELEASE();
	return net_recv(cos_spd_id(), nc, buf, sz);
}

static int fd_net_write(int fd, struct descriptor *d, char *buf, int sz)
{
	net_connection_t nc;

	assert(d->type == DESC_NET);
	nc = (net_connection_t)d->data;
	FD_LOCK_RELEASE();
	return net_send(cos_spd_id(), nc, buf, sz);
}

int cos_socket(int domain, int type, int protocol)
{
	net_connection_t nc;
	struct descriptor *d;
	int fd, ret;
	long evt_id;

	if (PF_INET != domain) return -EINVAL;
	if (0 != protocol)     return -EINVAL;

	FD_LOCK_TAKE();
	if (NULL == (d = fd_alloc(DESC_NET))) {
		ret = -EINVAL;
		goto err;
	}
	fd = fd_get_index(d);
	if (0 > (evt_id = evt_create(cos_spd_id()))) {
		printc("Could not create event for socket: %ld", evt_id);
		assert(0);
	}
	d->evt_id = evt_id;
	evt2fd_create(evt_id, d);

	d->ops.close = fd_net_close;
	d->ops.read  = fd_net_read;
	d->ops.write = fd_net_write;

	switch (type) {
	case SOCK_STREAM:
		nc = net_create_tcp_connection(cos_spd_id(), cos_get_thd_id(), evt_id);
		break;
	case SOCK_DGRAM:
		/* WARNING: its been an awfully long time since I tested udp...beware */
		assert(0);
		nc = net_create_udp_connection(cos_spd_id(), evt_id);
		break;
	default:
		ret = -EINVAL;
		goto err_cleanup;
	}
	if (nc < 0) {
		ret = nc;
		goto err_cleanup;
	}
	d->data = (void *)nc;
	FD_LOCK_RELEASE();

	return fd;
err_cleanup:
	evt_free(cos_spd_id(), d->evt_id);
	fd_free(d);
	/* fall through */
err:
	FD_LOCK_RELEASE();
	return ret;
}

int cos_listen(int fd, int queue)
{
	struct descriptor *d;
	net_connection_t nc;
	int ret;

	FD_LOCK_TAKE();
	d = fd_get_desc(fd);
	if (NULL == d) goto err;
	if (d->type != DESC_NET) goto err;
	nc = (net_connection_t)d->data;
	ret = net_listen(cos_spd_id(), nc, queue);
//	evt_set_prio(cos_spd_id(), d->evt_id, 1);
	FD_LOCK_RELEASE();

	return ret;
err:
	FD_LOCK_RELEASE();
	return -EBADFD;
}

int cos_bind(int fd, u32_t ip, u16_t port)
{
	struct descriptor *d;
	net_connection_t nc;
	int ret;

	FD_LOCK_TAKE();
	d = fd_get_desc(fd);
	if (NULL == d) goto err;
	if (d->type != DESC_NET) goto err;
	nc = (net_connection_t)d->data;
	ret = net_bind(cos_spd_id(), nc, ip, port);
	FD_LOCK_RELEASE();
	
	return ret;
err:
	FD_LOCK_RELEASE();
	return -EBADFD;
}

int cos_accept(int fd)
{
	struct descriptor *d, *d_new;
	net_connection_t nc_new, nc;
	long evt_id;

	do {
		FD_LOCK_TAKE();
		d = fd_get_desc(fd);
		if (NULL == d) goto err;
		if (d->type != DESC_NET) goto err;
		nc = (net_connection_t)d->data;
		FD_LOCK_RELEASE();
		nc_new = net_accept(cos_spd_id(), nc);
		if (-EAGAIN == nc_new) {
			return -EAGAIN;
			/* 
			 * Blocking accept: 
			 * if (evt_wait(cos_spd_id(), d->evt_id)) assert(0);
			 */
		} else if (nc_new < 0) {
			return nc_new;
		}
	} while (-EAGAIN == nc_new);

	FD_LOCK_TAKE();
	/* If this error is triggered, we should also close the nc */
	if (NULL == (d_new = fd_alloc(DESC_NET))) {
		net_close(cos_spd_id(), nc_new);
		FD_LOCK_RELEASE();
		return -ENOMEM;
	}

	d_new->ops.close = fd_net_close;
	d_new->ops.read  = fd_net_read;
	d_new->ops.write = fd_net_write;

	fd = fd_get_index(d_new);
	d_new->data = (void*)nc_new;
	evt_id = evt_create(cos_spd_id());
	if (0 > evt_id) {
		printc("cos_accept: evt_create (evt %d) failed with %ld", fd, evt_id);
		assert(0);
	}
	d_new->evt_id = evt_id;
	evt2fd_create(evt_id, d_new);
	/* Associate the net connection with the event value fd */
	if (0 < net_accept_data(cos_spd_id(), nc_new, evt_id)) assert(0);
	FD_LOCK_RELEASE();

	return fd;
err:
	FD_LOCK_RELEASE();
	return -EBADFD;
}

/* 
 * http (app specific) parsing specific services
 */
extern int content_write(spdid_t spdid, long connection_id, char *reqs, int sz);
extern int content_read(spdid_t spdid, long connection_id, char *buff, int sz);
extern long content_create(spdid_t spdid, long evt_id, struct cos_array *d);
extern int content_remove(spdid_t spdid, long conn_id);

static int fd_app_close(int fd, struct descriptor *d)
{
	long conn_id;

	assert(d->type == DESC_HTTP);

	conn_id = (long)d->data;
	content_remove(cos_spd_id(), conn_id);
	evt_free(cos_spd_id(), d->evt_id);
	FD_LOCK_RELEASE();
	
	FD_LOCK_TAKE();
	fd_free(d);
	FD_LOCK_RELEASE();

	return 0;
}

static int fd_app_read(int fd, struct descriptor *d, char *buff, int sz)
{
	long conn_id;

	assert(d->type == DESC_HTTP);
	conn_id = (long)d->data;
	FD_LOCK_RELEASE();
	return content_read(cos_spd_id(), conn_id, buff, sz);
}

static int fd_app_write(int fd, struct descriptor *d, char *buff, int sz)
{
	long conn_id;

	assert(d->type == DESC_HTTP);
	conn_id = (long)d->data;
	FD_LOCK_RELEASE();
	return content_write(cos_spd_id(), conn_id, buff, sz);
}

int cos_app_open(int type, struct cos_array *data)
{
	struct descriptor *d;
	int fd, ret = -EINVAL;
	long conn_id, evt_id;

	/* FIXME: ignoring type for now */

	FD_LOCK_TAKE();
	if (NULL == (d = fd_alloc(DESC_HTTP))) {
		ret = -ENOMEM;
		goto err;
	}
	fd = fd_get_index(d);
	if (0 > (evt_id = evt_create(cos_spd_id()))) {
		printc("Could not create event for app fd: %ld\n", evt_id);
		assert(0);
	}
	d->evt_id = evt_id;
	evt2fd_create(evt_id, d);
	d->ops.close = fd_app_close;
	d->ops.read  = fd_app_read;
	d->ops.write = fd_app_write;

	conn_id = content_create(cos_spd_id(), evt_id, data);
	if (conn_id < 0) goto err_cleanup;
	d->data = (void *)conn_id;
	FD_LOCK_RELEASE();

	return fd;
err_cleanup:
	evt_free(cos_spd_id(), d->evt_id);
	fd_free(d);
	/* fall through */
err:
	FD_LOCK_RELEASE();
	return ret;
	
}

/* 
 * Generic functions
 */
int cos_close(int fd)
{
	struct descriptor *d;

	FD_LOCK_TAKE();
	d = fd_get_desc(fd);
	if (NULL == d) goto err;
	return d->ops.close(fd, d);
err:
	FD_LOCK_RELEASE();
	return -EBADFD;
}

int cos_read(int fd, char *buf, int sz)
{
	struct descriptor *d;

	if (!cos_argreg_buff_intern(buf, sz)) return -EFAULT;
	FD_LOCK_TAKE();
	d = fd_get_desc(fd);
	if (NULL == d) goto err;

	return d->ops.read(fd, d, buf, sz);
err:
	FD_LOCK_RELEASE();
	return -EBADFD;
}

int cos_write(int fd, char *buf, int sz)
{
	struct descriptor *d;

	if (!cos_argreg_buff_intern(buf, sz)) return -EFAULT;
	FD_LOCK_TAKE();
	d = fd_get_desc(fd);
	if (NULL == d) goto err;

	return d->ops.write(fd, d, buf, sz);
err:
	FD_LOCK_RELEASE();
	return -EBADFD;
}

int cos_wait(int fd)
{
	struct descriptor *d;
	long evt_id;

	FD_LOCK_TAKE();
	d = fd_get_desc(fd);
	if (NULL == d) {
		FD_LOCK_RELEASE();
		return -1;
	}
	evt_id = d->evt_id;
	FD_LOCK_RELEASE();

	return evt_wait(cos_spd_id(), evt_id);
}

int cos_wait_all(void)
{
	long evt;
	struct descriptor *d;

	evt = evt_grp_wait(cos_spd_id());
	d = evt2fd_lookup(evt);
	assert(d);
	
	return d->fd_num;
}

static void init(void) 
{
	lock_static_init(&fd_lock);
	cos_map_init_static(&fds);
	cos_vect_init_static(&evt2fdesc);
//	sched_block(cos_spd_id());
}

void cos_init(void *arg)
{
	static volatile int first = 1;

	if (first) {
		first = 0;
		init();
	} else {
		prints("fd: not expecting more than one bootstrap.");
	}
}

void bin(void)
{
	sched_block(cos_spd_id());
}

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

#include <fd.h>

#include <sched.h>
#include <evt.h>
#include <net_transport.h>
#include <http.h>

typedef enum {
	DESC_TOP,
	DESC_NET,
	DESC_HTTP
} desc_t;

struct descriptor;
struct fd_ops {
	int (*close)(int fd, struct descriptor *d);
	int (*split)(struct descriptor *d);
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

/* 
 * Provide a cache of events that have happened to amortize the cost
 * of invoking the event server.
 */
#define EVT_NOTIF_CACHE_SZ 32
/* due to race conditions, we might expand much larger... */
#define EVT_NOTIF_CACHE_MAX 128
int evt_notif_top;
static long evt_notif_cache[EVT_NOTIF_CACHE_MAX];

/* can block...don't hold a lock */
static int fill_evt_notif_cache(void)
{
	int amnt;
	struct cos_array *data;
	
	data = cos_argreg_alloc((sizeof(long) * EVT_NOTIF_CACHE_SZ) + sizeof(struct cos_array));
	assert(data);
	data->sz = EVT_NOTIF_CACHE_SZ * sizeof(long);
	amnt = evt_grp_mult_wait(cos_spd_id(), data);
	if (amnt <= 0) BUG();

	assert(amnt <= EVT_NOTIF_CACHE_SZ);
	FD_LOCK_TAKE();
	/* too many race conditions! */
	assert(evt_notif_top + amnt < EVT_NOTIF_CACHE_MAX);
	memcpy(&evt_notif_cache[evt_notif_top], data->mem, amnt * sizeof(long));
	evt_notif_top += amnt;
	FD_LOCK_RELEASE();
	cos_argreg_free(data);

	return 0;
}

/*
 * Provide a cache for event ids so that we don't need to ask for new
 * ones and deallocate them all the time.  
 */
#define EVT_ID_CACHE_SZ 8
static long cached_ids[EVT_ID_CACHE_SZ];

static long evt_create_cached(spdid_t spdid)
{
	int i;

	for (i = 0 ; i < EVT_ID_CACHE_SZ ; i++) {
		if (cached_ids[i] >= 0) {
			long ret;
			ret = cached_ids[i];
			cached_ids[i] = -1;
			return ret;
		}
	}
	return evt_create(spdid);
}


static void evt_free_cached(spdid_t spdid, long evt_id)
{
	int i;

	/* remove this event id from the notification cache */
	for (i = 0 ; i < evt_notif_top ; i++) {
		int j;
		
		while (evt_notif_cache[i] == evt_id) {
			for (j = i ; j < evt_notif_top-1 ; j++) {
				evt_notif_cache[j] = evt_notif_cache[j+1];
			}
			evt_notif_top--;
		}
	}

	for (i = 0 ; i < EVT_ID_CACHE_SZ ; i++) {
		if (cached_ids[i] < 0) {
			cached_ids[i] = evt_id;
			return;
		}
	}
	evt_free(spdid, evt_id);
}

static struct descriptor *evt2fd_lookup(long evt_id)
{
	return cos_vect_lookup(&evt2fdesc, evt_id);
}

static void evt2fd_create(long evt_id, struct descriptor *d)
{
	assert(NULL == evt2fd_lookup(evt_id));
	if (0 > cos_vect_add_id(&evt2fdesc, d, evt_id)) BUG();
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

static int fd_net_close(int fd, struct descriptor *d)
{
	net_connection_t nc;
	int ret;

	assert(d->type == DESC_NET);
	nc = (net_connection_t)d->data;
	ret = net_close(cos_spd_id(), nc);
	evt_free_cached(cos_spd_id(), d->evt_id);
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
	if (0 > (evt_id = evt_create_cached(cos_spd_id()))) {
		printc("Could not create event for socket: %ld", evt_id);
		BUG();
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
		BUG();
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
	evt_free_cached(cos_spd_id(), d->evt_id);
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
			 * if (evt_wait(cos_spd_id(), d->evt_id)) BUG();
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
		BUG();
	}
	d_new->evt_id = evt_id;
	evt2fd_create(evt_id, d_new);
	/* Associate the net connection with the event value fd */
	if (0 < net_accept_data(cos_spd_id(), nc_new, evt_id)) BUG();
	FD_LOCK_RELEASE();

	return fd;
err:
	FD_LOCK_RELEASE();
	return -EBADFD;
}

static int fd_app_close(int fd, struct descriptor *d)
{
	long conn_id;

	assert(d->type == DESC_HTTP);

	conn_id = (long)d->data;
	content_remove(cos_spd_id(), conn_id);
	evt_free_cached(cos_spd_id(), d->evt_id);
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

static int fd_app_split(struct descriptor *d)
{
	struct descriptor *d_new;
	int fd, ret = -EINVAL;
	long conn_id, evt_id;

	if (NULL == (d_new = fd_alloc(DESC_HTTP))) {
		ret = -ENOMEM;
		goto err;
	}
	fd = fd_get_index(d_new);
	d_new->ops.close = fd_app_close;
	d_new->ops.read  = fd_app_read;
	d_new->ops.write = fd_app_write;
	d_new->ops.split = fd_app_split;

	if (0 > (evt_id = evt_create_cached(cos_spd_id()))) {
		printc("Could not create event for app fd: %ld\n", evt_id);
		BUG();
	}
	d_new->evt_id = evt_id;
	evt2fd_create(evt_id, d);

	conn_id = content_split(cos_spd_id(), (long)d->data, evt_id);
	if (conn_id < 0) {
		ret = conn_id;
		goto err_cleanup;
	}
	d_new->data = (void *)conn_id;

	FD_LOCK_RELEASE();

	return fd;
err_cleanup:
	evt_free_cached(cos_spd_id(), d_new->evt_id);
	fd_free(d_new);
	/* fall through */
err:
	FD_LOCK_RELEASE();
	return ret;
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
	if (0 > (evt_id = evt_create_cached(cos_spd_id()))) {
		printc("Could not create event for app fd: %ld\n", evt_id);
		BUG();
	}
	d->evt_id = evt_id;
	evt2fd_create(evt_id, d);
	d->ops.close = fd_app_close;
	d->ops.read  = fd_app_read;
	d->ops.write = fd_app_write;
	d->ops.split = fd_app_split;

	conn_id = content_create(cos_spd_id(), evt_id, data);
	if (conn_id < 0) goto err_cleanup;
	d->data = (void *)conn_id;
	FD_LOCK_RELEASE();

	return fd;
err_cleanup:
	evt_free_cached(cos_spd_id(), d->evt_id);
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

int cos_split(int fd)
{
	struct descriptor *d;

	FD_LOCK_TAKE();
	d = fd_get_desc(fd);
	if (NULL == d || NULL == d->ops.split) goto err;
	return d->ops.split(d);
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
	int fd;

	FD_LOCK_TAKE();
//#define CACHE_EVT_NOTIFICATIONS
#ifdef  CACHE_EVT_NOTIFICATIONS
	while (0 == evt_notif_top) {
		FD_LOCK_RELEASE();
		fill_evt_notif_cache();
		FD_LOCK_TAKE();
	}
	assert(evt_notif_top > 0);

	evt_notif_top--;
	evt = evt_notif_cache[evt_notif_top];
#else
	evt = evt_grp_wait(cos_spd_id());
#endif
	d = evt2fd_lookup(evt);
	assert(d);
	fd = d->fd_num;
	FD_LOCK_RELEASE();

	return fd;
}

static void init(void) 
{
	int i;

	lock_static_init(&fd_lock);
	cos_map_init_static(&fds);
	cos_vect_init_static(&evt2fdesc);

	for (i = 0 ; i < EVT_ID_CACHE_SZ ; i++) {
		cached_ids[i] = -1;
	}
	for (i = 0 ; i < EVT_NOTIF_CACHE_MAX ; i++) {
		evt_notif_cache[i] = -1;
	}
	evt_notif_top = 0;
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
	sched_block(cos_spd_id(), 0);
}

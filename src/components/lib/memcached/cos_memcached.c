#include <string.h>
#include <cos_component.h>
#include "cos_adapter/cos_mc_adapter.h"
#include "cos_memcached.h"
#include "cos_memcached_exp.h"

extern int cos_mc_main (int argc, char **argv);

static char *client_query = "\0\0\0\0\0\1\0\0set GWU_SYS 0 0 5\r\nGREAT\r\n";

int
cos_new_fd(enum network_transport transport)
{
	if (IS_TCP(transport)) {
		/* TODO: allocate fd, take care of multiple thread case */
		return 1;
	} else {
		return cos_thdid();;
	}

}

void
cos_free_fd(int fd)
{
	/* TODO: deallocate fd, take care of multiple thread case */
	return;
}

ssize_t
cos_tcp_read(conn *c, void *buf, size_t count)
{
	assert (c != NULL);

	memcpy(buf, c->cos_r_buf, c->cos_r_sz);

	return c->cos_r_sz;
}

/* Both TCP and UDP conn will write data back to c->cos_w_buf */
ssize_t
cos_sendmsg(void *c, struct msghdr *msg, int flags)
{
	conn *_c = (conn *)c;
	/* TODO: copy shm data from Memcached to shmem */
	assert (c != NULL);
	ssize_t sent_len = 0;

	for (ssize_t i = 0; i < msg->msg_iovlen; i++) {
		memcpy(_c->cos_w_buf + sent_len, msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len);
		sent_len += msg->msg_iov[i].iov_len;
	}

	/* sent data size cannot exceed buffer size */
	assert(sent_len < _c->cos_w_sz);
	_c->cos_w_sz = sent_len;

	return sent_len;
}

ssize_t
cos_tcp_write(conn *c, void *buf, size_t count)
{
	/* TODO: hack it if necessary */
	assert (c != NULL);
	return 0;
}

/* UDP connection uses this to copy packet data to its buffer */
ssize_t
cos_recvfrom(void *c)
{
	conn *_c = (conn *)c;
	assert (c != NULL);

	memcpy(_c->rbuf, _c->cos_r_buf, _c->cos_r_sz);
	return _c->cos_r_sz;
}

void*
cos_select_thd(void)
{
	return get_worker_thread(cos_thdid());
}

int
cos_mc_init(int argc, char **argv)
{
	int ret = 0;

	ret = cos_mc_main(argc, argv);

	return ret;
}

/* each thread needs to create a new connection once */
int
cos_mc_new_conn(int proto)
{
	int fd;
	conn *c;
	thdid_t tid;

	enum network_transport transport;

	tid = cos_thdid();

	/* need to make sure tid is withing the thread index range */
	assert(tid > 0 && tid < settings.num_threads);

	/* initialize libevent thread struct just needs to be done per thread/core */
	LIBEVENT_THREAD *me = get_worker_thread(tid);
	cos_mc_init_thd(me);

	transport = settings.udpport == 0 ? tcp_transport:udp_transport;

	/* make sure client use the same proto as memcached */
	assert(proto == transport);

	fd = cos_new_fd(transport);

	if(IS_TCP(transport)) {
		/*
		* A new connection will be initialized by the defaulter listener, mimic the mater thread
		* This will enqueue that connection into the "worker" thread
		*/
		dispatch_conn_new(fd, conn_new_cmd, 0, READ_BUFFER_CACHED, transport, NULL);
	} else {
		/* UDP is simply just a conn_read state, this will cause "worker thread" to create UDP conn */
		dispatch_conn_new(fd, conn_read, 0, UDP_READ_BUFFER_SIZE, transport, NULL);
	}

	/* This will dequeue the connection and create a connection struct for that connection */
	cos_mc_establish_conn(me);


	if (IS_TCP(transport)) {
		/* Reset conn read and write to Composite variant */
		c = cos_mc_get_conn(fd);

		c->read    = cos_tcp_read;
		c->sendmsg = (void *)cos_sendmsg;
		c->write   = cos_tcp_write;

		/* 
		* This will make the connection state to be conn_waiting,thus the following calls to
		* this event_handler will assume each time there will be some data arrives.
		*/
		cos_mc_event_handler(fd, cos_mc_get_conn(fd));
	}



	return fd;
}

u16_t
cos_mc_process_command(int fd, char *r_buf, u16_t r_buf_len, char *w_buf, u16_t w_buf_len)
{
	conn *c;
	c = cos_mc_get_conn(fd);

	c->cos_r_buf	= r_buf;
	c->cos_r_sz	= r_buf_len;
	c->cos_w_buf	= w_buf;
	c->cos_w_sz	= w_buf_len;

	cos_mc_event_handler(fd, c);

	return c->cos_w_sz;
}

void
mc_test(void)
{
	conn *c;
	int fd;

	/* Initialize a new connection and get a fd back */
	fd = cos_mc_new_conn(udp_transport);

	/* Assuming some data comes, trigger the event_hander to deal with it */
	cos_mc_event_handler(fd, cos_mc_get_conn(fd));

	client_query = "\0\0\0\0\0\1\0\0get GWU_SYS\r\n";

	cos_mc_event_handler(fd, cos_mc_get_conn(fd));
}

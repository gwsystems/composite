#include <string.h>
#include "cos_adapter/cos_mc_adapter.h"
#include "cos_memcached.h"
#include "cos_memcached_exp.h"

char *client_query = "set GWU_SYS 0 0 5\r\nGREAT\r\n";

int
cos_new_fd(void)
{
	/* TODO: allocate fd, take care of multiple thread case */
	return 1;
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
	/* TODO: copy shmem data to Memcached's buf */
	assert (c != NULL);
	memcpy(buf, client_query, strlen(client_query));
	return strlen(client_query);
}

ssize_t
cos_tcp_sendmsg(conn *c, struct msghdr *msg, int flags)
{
	/* TODO: copy shm data from Memcached to shmem */
	assert (c != NULL);
	ssize_t sent_len = 0;
	cos_printf("iov len:%d\n",msg->msg_iovlen);
	for (ssize_t i = 0; i < msg->msg_iovlen; i++) {
		sent_len += msg->msg_iov[i].iov_len;
		cos_printf("sent msg (sz: %d): %s\n",msg->msg_iov[i].iov_len, msg->msg_iov[i].iov_base);
	}
	return sent_len;
}

ssize_t
cos_tcp_write(conn *c, void *buf, size_t count)
{
	/* TODO: hack it if necessary */
	assert (c != NULL);
	return 0;
}

int
cos_mc_init(int argc, char **argv)
{
	int ret = 0;

	ret = cos_mc_main(argc, argv);

	return ret;
}

int
cos_mc_new_conn(void)
{
	int fd;
	conn *c;

	/* initialize libevent thread struct just needs to be done per thread/core */
	LIBEVENT_THREAD *me = get_worker_thread(0);
	cos_mc_init_thd(me);

	c = cos_mc_get_conn(COS_MC_LISTEN_FD);

	fd = cos_new_fd();

	/*
	 * A new connection will be initialized by the defaulter listener, mimic the mater thread
	 * This will enqueue that connection into the "worker" thread
	 */
	dispatch_conn_new(fd, conn_new_cmd, 0, READ_BUFFER_CACHED, c->transport, NULL);

	/* This will dequeue the connection and create a connection struct for that connection */
	cos_mc_establish_conn(me);

	c = cos_mc_get_conn(fd);

	/* Reset conn read and write to Composite variant */
	c->read = cos_tcp_read;
	c->sendmsg = cos_tcp_sendmsg;
	c->write = cos_tcp_write;

	/* 
	 * This will make the connection state to be conn_waiting,thus the following calls to
	 * this event_handler will assume each time there will be some data arrives.
	 */
	cos_mc_event_handler(fd, cos_mc_get_conn(fd));

	return fd;
}

void
mc_test(void)
{
	conn *c;
	int fd;

	/* Initialize a new connection and get a fd back */
	fd = cos_mc_new_conn();

	/* Assuming some data comes, trigger the event_hander to deal with it */
	cos_mc_event_handler(fd, cos_mc_get_conn(fd));

	client_query = "get GWU_SYS\r\n";

	cos_mc_event_handler(fd, cos_mc_get_conn(fd));
}

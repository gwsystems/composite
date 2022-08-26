#include <string.h>
#include <cos_component.h>
#include <netshmem.h>
#include "cos_adapter/cos_mc_adapter.h"
#include "cos_memcached.h"
#include "cos_memcached_exp.h"

char *client_query = "\0\0\0\0\0\1\0\0set GWU_SYS 0 0 5\r\nGREAT\r\n";

int
cos_new_fd(enum network_transport transport)
{
	if (IS_TCP(transport)) {
		/* TODO: allocate fd, take care of multiple thread case */
		return 1;
	} else {
		/* TODO: find the thread's conn fd is enough for UDP */
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
	/* TODO: copy shmem data to Memcached's buf */
	assert (c != NULL);
	memcpy(buf, client_query, strlen(client_query));
	return strlen(client_query);
}

ssize_t
cos_sendmsg(conn *c, struct msghdr *msg, int flags)
{
	/* TODO: copy shm data from Memcached to shmem */
	assert (c != NULL);
	ssize_t sent_len = 0;
	shm_bm_t shm = netshmem_get_shm();
	struct netshmem_pkt_buf *pkt_buf = shm_bm_take_net_pkt_buf(shm, c->shm_objid);
	char w_buf = netshmem_get_data_buf(pkt_buf);

	cos_printf("iov len:%d\n",msg->msg_iovlen);
	for (ssize_t i = 0; i < msg->msg_iovlen; i++) {
		sent_len += msg->msg_iov[i].iov_len;
		memcpy(w_buf + sent_len, msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len);
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

/* UDP connection uses this to copy packet data to its buffer */
ssize_t
cos_recvfrom(conn *c)
{
	assert (c != NULL);
	// ssize_t len = strlen(client_query + 8) + 8;
	shm_bm_t shm = netshmem_get_shm();
	char *data_buf = (char*)shm_bm_take_net_pkt_buf(shm, c->shm_objid);

	memcpy(c->rbuf, data_buf, c->shm_data_sz);
	return c->shm_data_sz;
}

LIBEVENT_THREAD*
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
cos_mc_new_conn(void)
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

	// c = cos_mc_get_conn(COS_MC_LISTEN_FD);
	transport = settings.udpport == 0 ? tcp_transport:udp_transport;

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

		c->read = cos_tcp_read;
		c->sendmsg = cos_sendmsg;
		c->write = cos_tcp_write;

		/* 
		* This will make the connection state to be conn_waiting,thus the following calls to
		* this event_handler will assume each time there will be some data arrives.
		*/
		cos_mc_event_handler(fd, cos_mc_get_conn(fd));
	}



	return fd;
}

void
cos_mc_process_command(int fd, shm_bm_objid_t objid, u16_t data_offset, u16_t data_len)
{
	conn *c;
	c = cos_mc_get_conn(fd);

	c->shm_objid = objid;
	c->shm_data_offset = data_offset;
	c->shm_data_sz = data_len;


	cos_mc_event_handler(fd, c);
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

	client_query = "\0\0\0\0\0\1\0\0get GWU_SYS\r\n";

	cos_mc_event_handler(fd, cos_mc_get_conn(fd));
}

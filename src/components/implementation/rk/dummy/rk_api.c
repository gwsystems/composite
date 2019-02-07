/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#include <cos_debug.h>
#include <rk.h>
#include <rk_types.h>
#include <sinv_async.h>
#include <stdlib.h>
#include <string.h>

static struct sinv_async_info sinv_info;
static int key_offset = 0, instance_key = 0;

int
test_entry(int arg1, int arg2, int arg3, int arg4)
{
	/* Not even sure why we still keep unused or stupid API!! anyway, arg4 ignored */
	return sinv_client_call(&sinv_info, TEST_ENTRY, arg1, arg2, arg3);
}

int
test_fs(int arg1, int arg2, int arg3, int arg4)
{
	/* Not even sure why we still keep unused or stupid API!! anyway, arg4 ignored */
	return sinv_client_call(&sinv_info, TEST_FS, arg1, arg2, arg3);
}

int
get_boot_done(void)
{
	/* Not even sure why we still keep unused or stupid API!! */
	return sinv_client_call(&sinv_info, GET_BOOT_DONE, 0, 0, 0);
}

int
rk_init(int shmid)
{
	return sinv_client_call(&sinv_info, RK_INIT, shmid, 0, 0);
}

int
rk_socket(int domain, int type, int protocol)
{
	return sinv_client_call(&sinv_info, RK_SOCKET, domain, type, protocol);
}

int
rk_connect(int fd, int shmid, unsigned addrlen)
{
	return sinv_client_call(&sinv_info, RK_CONNECT, fd, shmid, addrlen);
}

int
rk_bind(int socketfd, int shdmem_id, unsigned addrlen)
{
	return sinv_client_call(&sinv_info, RK_BIND, socketfd, shdmem_id, addrlen);
}

ssize_t
rk_recvfrom(int arg1, int arg2, int arg3)
{
	return sinv_client_call(&sinv_info, RK_RECVFROM, arg1, arg2, arg3);
}

ssize_t
rk_sendto(int arg1, int arg2, int arg3)
{
	return sinv_client_call(&sinv_info, RK_SENDTO, arg1, arg2, arg3);
}

int
rk_setsockopt(int arg1, int arg2, int arg3)
{
	return sinv_client_call(&sinv_info, RK_SETSOCKOPT, arg1, arg2, arg3);
}

int
rk_getsockopt(int sockfd_shmid, int level, int optname)
{
	return sinv_client_call(&sinv_info, RK_GETSOCKOPT, sockfd_shmid, level, optname);
}

void *
rk_mmap(int arg1, int arg2, int arg3)
{
	return (void *)sinv_client_call(&sinv_info, RK_MMAP, arg1, arg2, arg3);
}

long
rk_write(int arg1, int arg2, int arg3)
{
	return sinv_client_call(&sinv_info, RK_WRITE, arg1, arg2, arg3);
}

int
rk_fcntl(int arg1, int arg2, int arg3)
{
	return sinv_client_call(&sinv_info, RK_FCNTL, arg1, arg2, arg3);
}

ssize_t
rk_writev(int fd, int iovcnt, int shmid)
{
	return sinv_client_call(&sinv_info, RK_WRITEV, fd, iovcnt, shmid);
}

long
rk_read(int arg1, int arg2, int arg3)
{
	return sinv_client_call(&sinv_info, RK_READ, arg1, arg2, arg3);
}

int
rk_listen(int arg1, int arg2)
{
	return sinv_client_call(&sinv_info, RK_LISTEN, arg1, arg2, 0);
}

int
rk_clock_gettime(int arg1, int arg2)
{
	return sinv_client_call(&sinv_info, RK_CLOCK_GETTIME, arg1, arg2, 0);
}

int
rk_select(int arg1, int arg2)
{
	return sinv_client_call(&sinv_info, RK_SELECT, arg1, arg2, 0);
}

int
rk_accept(int arg1, int arg2)
{
	return sinv_client_call(&sinv_info, RK_ACCEPT, arg1, arg2, 0);
}

int
rk_open(int arg1, int arg2, int arg3)
{
	return sinv_client_call(&sinv_info, RK_OPEN, arg1, arg2, arg3);
}

int
rk_close(int fd)
{
	return sinv_client_call(&sinv_info, RK_CLOSE, fd, 0, 0);
}

int
rk_unlink(int arg1)
{
	return sinv_client_call(&sinv_info, RK_UNLINK, arg1, 0, 0);
}

int
rk_ftruncate(int arg1, int arg2)
{
	return sinv_client_call(&sinv_info, RK_FTRUNCATE, arg1, arg2, 0);
}

int
rk_getsockname(int arg1, int arg2)
{
	return sinv_client_call(&sinv_info, RK_GETSOCKNAME, arg1, arg2, 0);
}

int
rk_getpeername(int arg1, int arg2)
{
	return sinv_client_call(&sinv_info, RK_GETPEERNAME, arg1, arg2, 0);
}

void
rk_dummy_init(void)
{
	instance_key = rk_args_instance();
	assert(instance_key > 0);

	sinv_client_init(&sinv_info, RK_CLIENT(instance_key));
}

int
rk_dummy_thdinit(thdid_t tid, int is_aep)
{
	int ret;
	int off = key_offset;

	ps_faa((unsigned long *)&key_offset, 1);
	ret = sinv_client_thread_init(&sinv_info, tid, is_aep ? 0 : RK_RKEY(instance_key, off),
				      RK_SKEY(instance_key, off));
	assert(ret == 0);

	return ret;
}

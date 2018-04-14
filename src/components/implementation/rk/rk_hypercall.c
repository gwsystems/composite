#include <cos_kernel_api.h>
#include <cos_types.h>
#include <cringbuf.h>
#include <sinv_calls.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <rumpcalls.h>
#include <vk_types.h>
#include <llprint.h>
#include <rk.h>
#include <memmgr.h>

int     rump___sysimpl_socket30(int, int, int);
int     rump___sysimpl_bind(int, const struct sockaddr *, socklen_t);
ssize_t rump___sysimpl_recvfrom(int, void *, size_t, int, struct sockaddr *, socklen_t *);
ssize_t rump___sysimpl_sendto(int, const void *, size_t, int, const struct sockaddr *, socklen_t);
int     rump___sysimpl_setsockopt(int, int, int, const void *, socklen_t);
int     rump___sysimpl_listen(int, int);
int     rump___sysimpl_clock_gettime50(clockid_t, struct timespec *);
int     rump___sysimpl_select50(int nd, fd_set *, fd_set *, fd_set *, struct timeval *);
int     rump___sysimpl_accept(int, struct sockaddr *, socklen_t *);
int     rump___sysimpl_getsockname(int, struct sockaddr *, socklen_t *);
int     rump___sysimpl_getpeername(int, struct sockaddr *, socklen_t *);
int     rump___sysimpl_open(const char *, int, mode_t);
int     rump___sysimpl_unlink(const char *);
int     rump___sysimpl_ftruncate(int, off_t);
ssize_t rump___sysimpl_write(int, const void *, size_t);
ssize_t rump___sysimpl_read(int, const void *, size_t);
void   *rump_mmap(void *, size_t, int, int, int, off_t);

/* These synchronous invocations involve calls to and from a RumpKernel */
//extern struct cringbuf *vmrb;
/* TODO when rumpbooter is its own interface, have this as an exported symbol */
struct cringbuf *vmrb = NULL;

int
test_entry(int arg1, int arg2, int arg3, int arg4)
{
        int ret = 0;

        printc("\n*** KERNEL COMPONENT ***\n \tArguments: %d, %d, %d, %d\n", arg1, arg2, arg3, arg4);
        printc("spdid: %d\n", arg4);
        printc("*** KERNEL COMPONENT RETURNING ***\n\n");

        return ret;
}

int
test_fs(int arg1, int arg2, int arg3, int arg4)
{
        int ret = 0;

        printc("\n*** KERNEL COMPONENT ***\n \tArguments: %d, %d, %d, %d\n", arg1, arg2, arg3, arg4);
        printc("spdid: %d\n", arg4);

        /* FS Test */
        printc("Running paws test: VM%d\n", cos_spdid_get());
//      paws_tests();

        printc("*** KERNEL COMPONENT RETURNING ***\n\n");

        return ret;

}

int
rk_ftruncate(int arg1, int arg2)
{ return rump___sysimpl_ftruncate(arg1, (off_t)arg2); }

int
rk_select(int arg1, int arg2)
{

	int nd = arg1, ret;
	int shdmem_id = arg2;
	static int old_shdmem_id = -1;
	static vaddr_t buf = 0;
	vaddr_t tmp;
	fd_set *in = NULL, *ou = NULL, *ex = NULL;
	struct timeval *tv = NULL;
	int *null_array;

	if (old_shdmem_id != shdmem_id || !buf) {
		old_shdmem_id = shdmem_id;
		ret = memmgr_shared_page_map(shdmem_id, &buf);
		assert(ret);
	}

	assert(buf && (old_shdmem_id == shdmem_id));

	null_array = buf;
	tmp = null_array + 4;

	if (null_array[0]) in = (fd_set *)tmp;
	tmp += sizeof(fd_set);
	if (null_array[1]) ou = (fd_set *)tmp;
	tmp += sizeof(fd_set);
	if (null_array[2]) ex = (fd_set *)tmp;
	tmp += sizeof(fd_set);
	if (null_array[3]) tv = (struct timeval *)tmp;

	return rump___sysimpl_select50(nd, in, ou, ex, tv);
}

int
rk_accept(int arg1, int arg2)
{

	int s = arg1, ret;
	int shdmem_id = arg2;
	static int old_shdmem_id = -1;
	static vaddr_t buff = 0;
	vaddr_t tmp;
	struct sockaddr *name;
	socklen_t *anamelen;

	if (old_shdmem_id != shdmem_id || !buff) {
		old_shdmem_id = shdmem_id;
		ret = memmgr_shared_page_map(shdmem_id, &buff);
		assert(ret);
	}

	assert(buff && (old_shdmem_id == shdmem_id));

	tmp = buff;
	name = (struct sockaddr *)tmp;
	tmp += sizeof(struct sockaddr);
	anamelen = (socklen_t *)tmp;

	return rump___sysimpl_accept(s, name, anamelen);
}

int
get_boot_done(void) {
	return 1;
}

int
rk_socket(int domain, int type, int protocol)
{
	printc("RK socket\n");
	return rump___sysimpl_socket30(domain, type, protocol);
}

int
rk_open(int arg1, int arg2, int arg3)
{
	int shdmem_id, ret, flags;
	mode_t mode;
	const char *path;
	static const void *buf = NULL;
	static int old_shdmem_id = -1;

	shdmem_id = arg1;
	flags     = arg2;
	mode    = (mode_t)arg3;

	if (buf == NULL || old_shdmem_id != shdmem_id) {
		old_shdmem_id = shdmem_id;
		ret = memmgr_shared_page_map(shdmem_id, &buf);
		assert(ret);
	}

	assert(buf && (shdmem_id == old_shdmem_id));

	path = buf;

	printc("path: %s, flags: %d, mode: %d\n", path, flags, mode);
	return rump___sysimpl_open(path, flags, mode);
}

int
rk_unlink(int arg1)
{
	int shdmem_id, ret;
	static int old_shdmem_id = -1;
	const char *path;
	static const void *buf = NULL;

	shdmem_id = arg1;

	if (buf == NULL || old_shdmem_id != shdmem_id) {
		old_shdmem_id = shdmem_id;
		ret = memmgr_shared_page_map(shdmem_id, &buf);
		assert(ret);
	}

	assert(buf && (shdmem_id == old_shdmem_id));

	path = buf;
	printc("path: %s\n", path);
	return rump___sysimpl_unlink(path);
}

int
rk_bind(int sockfd, int shdmem_id, socklen_t socklen)
{
	printc("RK bind\n");
	const struct sockaddr *sock = NULL;
	int ret;
	vaddr_t addr;
	ret = memmgr_shared_page_map(shdmem_id, &addr);
	assert(ret > -1 && addr);
	sock = (const struct sockaddr *)addr;
	return rump___sysimpl_bind(sockfd, sock, socklen);
}

ssize_t
rk_recvfrom(int arg1, int arg2, int arg3)
{
	/*
	 * TODO, simplify this, this is so ugly because it combines two functions that now
	 * don't need to be separated
	 */
	static int shdmem_id = -1;
	static vaddr_t my_addr;
	vaddr_t my_addr_tmp;
	void *buff;
	struct sockaddr *from;
	socklen_t *from_addr_len_ptr;
	int s, buff_shdmem_id, flags, from_shdmem_id, from_addr_len, ret;
	size_t len;

	printc("RK recvfrom\n");

	s = (arg1 >> 16);
	buff_shdmem_id = (arg1 << 16) >> 16;
	len = (arg2 >> 16);
	flags = (arg2 << 16) >> 16;
	from_shdmem_id = (arg3 >> 16);
	from_addr_len = (arg3 << 16) >> 16;

	if (shdmem_id == -1 && my_addr == 0) {
		shdmem_id = buff_shdmem_id;
		ret = memmgr_shared_page_map(buff_shdmem_id, &my_addr);
		assert(ret);
	}

	assert(shdmem_id > -1);
	assert(my_addr > 0);
	/* We are using only one page, make sure the id is the same */
	assert(buff_shdmem_id == from_shdmem_id && buff_shdmem_id == shdmem_id);

	/* TODO, put this in a function */
	/* In the shared memory page, first comes the message buffer for len amount */
	my_addr_tmp = my_addr;
	buff = (void *)my_addr_tmp;
	my_addr_tmp += len;

	/* Second is from addr length ptr */
	from_addr_len_ptr  = (void *)my_addr_tmp;
	*from_addr_len_ptr = from_addr_len;
	my_addr_tmp += sizeof(socklen_t *);

	/* Last is the from socket address */
	from = (struct sockaddr *)my_addr_tmp;

	return rump___sysimpl_recvfrom(s, buff, len, flags, from, from_addr_len_ptr);
}

ssize_t
rk_sendto(int arg1, int arg2, int arg3)
{
	static int shdmem_id = -1;
	static const void *buff;
	const struct sockaddr *sock;
	vaddr_t addr;
	int sockfd, flags, buff_shdmem_id, addr_shdmem_id, ret;
	size_t len;
	socklen_t addrlen;

	sockfd            = (arg1 >> 16);
	buff_shdmem_id    = (arg1 << 16) >> 16;
	len               = (arg2 >> 16);
	flags             = (arg2 << 16) >> 16;
	addr_shdmem_id    = (arg3 >> 16);
	addrlen           = (arg3 << 16) >> 16;

	printc("RK sendto\n");

	if (shdmem_id == -1 && buff == 0) {
		shdmem_id = buff_shdmem_id;
		ret = memmgr_shared_page_map(buff_shdmem_id, &addr);
		assert(ret);
		buff = (const void *)addr;
	}

	assert(shdmem_id > -1);
	assert(buff);
	assert(buff_shdmem_id == addr_shdmem_id && buff_shdmem_id == shdmem_id);

	sock = (const struct sockaddr *)(buff + len);
	assert(sock);

	return rump___sysimpl_sendto(sockfd, buff, len, flags, sock, addrlen);
}

int
rk_setsockopt(int arg1, int arg2, int arg3)
{
	int sockfd, level, optname, shdmem_id, ret;
	static void *optval = NULL;
	static int old_shdmem_id = -1;
	socklen_t optlen;

	sockfd     = (arg1 >> 16);
	level      = (arg1 << 16) >> 16;
	optname    = (arg2 >> 16);
	shdmem_id  = (arg2 << 16) >> 16;
	optlen     = arg3;

	if (optval == NULL || old_shdmem_id != shdmem_id) {
		old_shdmem_id = shdmem_id;
		ret = memmgr_shared_page_map(shdmem_id, &optval);
		assert(ret);
	}

	assert(optval && (shdmem_id == old_shdmem_id));

	return rump___sysimpl_setsockopt(sockfd, level, optname, (const void *)optval, optlen);
}

void *
rk_mmap(int arg1, int arg2, int arg3)
{
	void *addr, *ret;
	size_t len;
	int prot, flags, fd;
	off_t off;

	addr  = (void *)(arg1 >> 16);
	len   = (size_t)((arg1 << 16) >> 16);
	prot  = arg2 >> 16;
	flags = (arg2 << 16) >> 16;
	fd    = arg3 >> 16;
	off   = (off_t)((arg3 << 16) >> 16);

	ret = rump_mmap(addr, len, prot, flags, fd, off);

	return ret;
}

long
rk_write(int arg1, int arg2, int arg3)
{
	int fd, shdmem_id, ret;
	static const void *buf = NULL;
	static int old_shdmem_id = -1;
	size_t nbyte;

	fd        = arg1;
	shdmem_id = arg2;
	nbyte     = (size_t)arg3;

	if (buf == NULL || old_shdmem_id != shdmem_id) {
		old_shdmem_id = shdmem_id;
		ret = memmgr_shared_page_map(shdmem_id, &buf);
		assert(ret);
	}

	assert(buf && (shdmem_id == old_shdmem_id));

	return (long)rump___sysimpl_write(fd, buf, nbyte);
}

long
rk_read(int arg1, int arg2, int arg3)
{
	int fd, shdmem_id, ret;
	static const void *buf = NULL;
	static int old_shdmem_id = -1;
	size_t nbyte;

	fd        = arg1;
	shdmem_id = arg2;
	nbyte     = (size_t)arg3;

	if (buf == NULL || old_shdmem_id != shdmem_id) {
		old_shdmem_id = shdmem_id;
		ret = memmgr_shared_page_map(shdmem_id, &buf);
		assert(ret);
	}

	assert(buf && (shdmem_id == old_shdmem_id));

	return (long)rump___sysimpl_read(fd, buf, nbyte);
}

int
rk_listen(int arg1, int arg2)
{
	int s, backlog;

	s       = arg1;
	backlog = arg2;

	return rump___sysimpl_listen(s, backlog);
}

int
rk_clock_gettime(int arg1, int arg2)
{
	int shdmem_id, ret;
	clockid_t clock_id;
	static struct timespec *tp = NULL;
	static int old_shdmem_id = -1;

	clock_id  = (clockid_t)arg1;
	shdmem_id = arg2;

	/* TODO, make this a function */
	if (tp == NULL || old_shdmem_id != shdmem_id) {
		old_shdmem_id = shdmem_id;
		ret = memmgr_shared_page_map(shdmem_id, &tp);
		assert(ret);
	}

	assert(tp && (shdmem_id == old_shdmem_id));

	/* Per process clock 2 is not supported, user real-time clock */
	if (clock_id == 2) clock_id = 0;

	return rump___sysimpl_clock_gettime50(clock_id, tp);
}

int
rk_getsockname(int arg1, int arg2)
{
	int shdmem_id, ret, fdes;
	static int old_shdmem_id = -1;
	static vaddr_t buf = 0;
	struct sockaddr *asa;
	socklen_t *alen;
	vaddr_t tmp;

	fdes      = arg1;
	shdmem_id = arg2;

	/* TODO, make this a function */
	if (old_shdmem_id != shdmem_id) {
		old_shdmem_id = shdmem_id;
		ret = memmgr_shared_page_map(shdmem_id, &buf);
		assert(ret);
	}

	assert(buf && (shdmem_id == old_shdmem_id));

	tmp  = buf;
	asa  = (struct sockaddr *)tmp;
	tmp += sizeof(struct sockaddr);
	alen = (socklen_t *)tmp;

	return rump___sysimpl_getsockname(fdes, asa, alen);
}

int
rk_getpeername(int arg1, int arg2)
{
	int shdmem_id, ret, fdes;
	static int old_shdmem_id = -1;
	static vaddr_t buf = 0;
	struct sockaddr *asa;
	socklen_t *alen;
	vaddr_t tmp;

	fdes      = arg1;
	shdmem_id = arg2;

	/* TODO, make this a function */
	if (old_shdmem_id != shdmem_id) {
		old_shdmem_id = shdmem_id;
		ret = memmgr_shared_page_map(shdmem_id, &buf);
		assert(ret);
	}

	assert(buf && (shdmem_id == old_shdmem_id));

	tmp  = buf;
	asa  = (struct sockaddr *)tmp;
	tmp += sizeof(struct sockaddr);
	alen = (socklen_t *)tmp;

	return rump___sysimpl_getpeername(fdes, asa, alen);
}

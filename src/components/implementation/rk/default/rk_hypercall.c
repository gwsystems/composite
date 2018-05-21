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

static vaddr_t
rk_thdcalldata_shm_map(cbuf_t id)
{
	static cbuf_t id_calldata[MAX_NUM_THREADS] = { 0 };
	static vaddr_t addr_calldata[MAX_NUM_THREADS] = { 0 };
	unsigned long npages = 0;

	assert(id);
	if (unlikely(id_calldata[cos_thdid()] == 0)) {
		npages = memmgr_shared_page_map(id, &addr_calldata[cos_thdid()]);
		assert(npages >= 1);

		id_calldata[cos_thdid()] = id;
	}

	assert(id_calldata[cos_thdid()] && addr_calldata[cos_thdid()]);

	return addr_calldata[cos_thdid()];
}

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
{
	return rump___sysimpl_ftruncate(arg1, (off_t)arg2);
}

int
rk_select(int arg1, int arg2)
{
	int nd = arg1, ret;
	int shdmem_id = arg2;
	static int old_shdmem_id = 0;
	static vaddr_t buf = 0;
	vaddr_t tmp;
	fd_set *in = NULL, *ou = NULL, *ex = NULL;
	struct timeval *tv = NULL;
	int *null_array;

	if (old_shdmem_id != shdmem_id || !buf) {
		old_shdmem_id = shdmem_id;
		buf = rk_thdcalldata_shm_map(shdmem_id);
		assert(buf);
	}

	assert(buf && shdmem_id > 0 && (old_shdmem_id == shdmem_id));

	null_array = (int *)buf;
	tmp = (vaddr_t)null_array + (sizeof(int) * 4);

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
	static int old_shdmem_id = 0;
	static vaddr_t buff = 0;
	vaddr_t tmp;
	struct sockaddr *name;
	socklen_t *anamelen;

	if (old_shdmem_id != shdmem_id || !buff) {
		old_shdmem_id = shdmem_id;
		buff = rk_thdcalldata_shm_map(shdmem_id);
		assert(buff);
	}

	assert(buff && shdmem_id > 0 && (old_shdmem_id == shdmem_id));

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
	return rump___sysimpl_socket30(domain, type, protocol);
}

/* going with robbie's lead.. rk has different flag values?? define them here for mappings */
#define RK_O_RDONLY     0x00000000
#define RK_O_WRONLY     0x00000001
#define RK_O_RDWR       0x00000002
#define RK_O_CREAT      0x00000200
#define RK_O_EXCL       0x00000800
#define RK_O_NOCTTY     0x00008000
#define RK_O_TRUNC      0x00000400
#define RK_O_APPEND     0x00000008
#define RK_O_NONBLOCK   0x00000004
#define RK_O_DSYNC      0x00010000
#define RK_O_SYNC       0x00000080
#define RK_O_RSYNC      0x00020000
#define RK_O_DIRECTORY  0x00200000
#define RK_O_NOFOLLOW   0x00000100
#define RK_O_CLOEXEC    0x00400000
#define RK_O_ASYNC      0x00000040
#define RK_O_DIRECT     0x00080000
#define RK_O_NDELAY RK_O_NONBLOCK

#define MUSL_O_RDONLY         00
#define MUSL_O_WRONLY         01
#define MUSL_O_RDWR           02
#define MUSL_O_CREAT        0100
#define MUSL_O_EXCL         0200
#define MUSL_O_NOCTTY       0400
#define MUSL_O_TRUNC       01000
#define MUSL_O_APPEND      02000
#define MUSL_O_NONBLOCK    04000
#define MUSL_O_DSYNC      010000
#define MUSL_O_SYNC     04010000
#define MUSL_O_RSYNC    04010000
#define MUSL_O_DIRECTORY 0200000
#define MUSL_O_NOFOLLOW  0400000
#define MUSL_O_CLOEXEC  02000000
#define MUSL_O_ASYNC      020000
#define MUSL_O_DIRECT     040000
#define MUSL_O_NDELAY MUSL_O_NONBLOCK

static int
rk_open_flag(int muslflag)
{
	int rkflag = 0;

	if (muslflag & MUSL_O_RDONLY)    rkflag |= RK_O_RDONLY;
	if (muslflag & MUSL_O_WRONLY)    rkflag |= RK_O_WRONLY;
	if (muslflag & MUSL_O_RDWR)      rkflag |= RK_O_RDWR;
	if (muslflag & MUSL_O_CREAT)     rkflag |= RK_O_CREAT;
	if (muslflag & MUSL_O_EXCL)      rkflag |= RK_O_EXCL;
	if (muslflag & MUSL_O_NOCTTY)    rkflag |= RK_O_NOCTTY;
	if (muslflag & MUSL_O_TRUNC)     rkflag |= RK_O_TRUNC;
	if (muslflag & MUSL_O_APPEND)    rkflag |= RK_O_APPEND;
	if (muslflag & MUSL_O_NONBLOCK)  rkflag |= RK_O_NONBLOCK;
	if (muslflag & MUSL_O_DSYNC)     rkflag |= RK_O_DSYNC;
	if (muslflag & MUSL_O_SYNC)      rkflag |= RK_O_SYNC;
	if (muslflag & MUSL_O_RSYNC)     rkflag |= RK_O_RSYNC;
	if (muslflag & MUSL_O_DIRECTORY) rkflag |= RK_O_DIRECTORY;
	if (muslflag & MUSL_O_NOFOLLOW)  rkflag |= RK_O_NOFOLLOW;
	if (muslflag & MUSL_O_CLOEXEC)   rkflag |= RK_O_CLOEXEC;
	if (muslflag & MUSL_O_ASYNC)     rkflag |= RK_O_ASYNC;
	if (muslflag & MUSL_O_DIRECT)    rkflag |= RK_O_DIRECT;
	if (muslflag & MUSL_O_NDELAY)    rkflag |= RK_O_NDELAY;

	return rkflag;
}

int
rk_open(int arg1, int arg2, int arg3)
{
	int shdmem_id, ret, flags;
	mode_t mode;
	const char *path;
	static const void *buf = NULL;
	static int old_shdmem_id = 0;

	shdmem_id = arg1;
	flags     = arg2;
	mode    = (mode_t)arg3;

	if (buf == NULL || old_shdmem_id != shdmem_id) {
		old_shdmem_id = shdmem_id;
		buf = (void *)rk_thdcalldata_shm_map(shdmem_id);
		assert(buf);
	}

	assert(buf && shdmem_id > 0 && (shdmem_id == old_shdmem_id));

	path = buf;

	flags = rk_open_flag(flags);
	return rump___sysimpl_open(path, flags, mode);
}

int
rk_unlink(int arg1)
{
	int shdmem_id, ret;
	static int old_shdmem_id = 0;
	const char *path;
	static const void *buf = NULL;

	shdmem_id = arg1;

	if (buf == NULL || old_shdmem_id != shdmem_id) {
		old_shdmem_id = shdmem_id;
		buf = (void *)rk_thdcalldata_shm_map(shdmem_id);
		assert(buf);
	}

	assert(buf && shdmem_id > 0 && (shdmem_id == old_shdmem_id));

	path = buf;
	return rump___sysimpl_unlink(path);
}

int
rk_bind(int sockfd, int shdmem_id, socklen_t socklen)
{
	const struct sockaddr *sock = NULL;
	int ret = 0;
	vaddr_t addr;
	ret = memmgr_shared_page_map(shdmem_id, &addr);
	assert(ret > 0 && addr);
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
	static int shdmem_id = 0;
	static vaddr_t my_addr = 0;
	vaddr_t my_addr_tmp;
	void *buff;
	struct sockaddr *from;
	socklen_t *from_addr_len_ptr;
	int s, buff_shdmem_id, flags, from_shdmem_id, from_addr_len, ret;
	size_t len;

	s = (arg1 >> 16);
	buff_shdmem_id = (arg1 << 16) >> 16;
	len = (arg2 >> 16);
	flags = (arg2 << 16) >> 16;
	from_shdmem_id = (arg3 >> 16);
	from_addr_len = (arg3 << 16) >> 16;

	if (shdmem_id == 0 && my_addr == 0) {
		shdmem_id = buff_shdmem_id;
		my_addr = rk_thdcalldata_shm_map(shdmem_id);
		assert(my_addr);
	}

	assert(shdmem_id > 0);
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
	static int shdmem_id = 0;
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

	if (shdmem_id == 0 && buff == 0) {
		shdmem_id = buff_shdmem_id;
		addr = rk_thdcalldata_shm_map(shdmem_id);
		assert(addr);
		buff = (const void *)addr;
	}

	assert(shdmem_id > 0);
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
	static int old_shdmem_id = 0;
	socklen_t optlen;

	sockfd     = (arg1 >> 16);
	level      = (arg1 << 16) >> 16;
	optname    = (arg2 >> 16);
	shdmem_id  = (arg2 << 16) >> 16;
	optlen     = arg3;

	if (optval == NULL || old_shdmem_id != shdmem_id) {
		old_shdmem_id = shdmem_id;
		optval = (void *)rk_thdcalldata_shm_map(shdmem_id);
		assert(optval);
	}

	assert(optval && shdmem_id > 0 && (shdmem_id == old_shdmem_id));

	if (level == -1) level = 65535;

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
	static int old_shdmem_id = 0;
	size_t nbyte;

	fd        = arg1;
	shdmem_id = arg2;
	nbyte     = (size_t)arg3;

	if (buf == NULL || old_shdmem_id != shdmem_id) {
		old_shdmem_id = shdmem_id;
		buf = (void *)rk_thdcalldata_shm_map(shdmem_id);
		assert(buf);
	}

	assert(buf && shdmem_id > 0 && (shdmem_id == old_shdmem_id));

	return (long)rump___sysimpl_write(fd, buf, nbyte);
}

long
rk_read(int arg1, int arg2, int arg3)
{
	int fd, shdmem_id, ret;
	static const void *buf = NULL;
	static int old_shdmem_id = 0;
	size_t nbyte;

	fd        = arg1;
	shdmem_id = arg2;
	nbyte     = (size_t)arg3;

	if (buf == NULL || old_shdmem_id != shdmem_id) {
		old_shdmem_id = shdmem_id;
		buf = (void *)rk_thdcalldata_shm_map(shdmem_id);
		assert(buf);
	}

	assert(buf && shdmem_id > 0 && (shdmem_id == old_shdmem_id));

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
	static int old_shdmem_id = 0;

	clock_id  = (clockid_t)arg1;
	shdmem_id = arg2;

	/* TODO, make this a function */
	if (tp == NULL || old_shdmem_id != shdmem_id) {
		old_shdmem_id = shdmem_id;
		tp = (struct timespec *)rk_thdcalldata_shm_map(shdmem_id);
		assert(tp);
	}

	assert(tp && shdmem_id > 0 && (shdmem_id == old_shdmem_id));

	/* Per process clock 2 is not supported, user real-time clock */
	if (clock_id == 2) clock_id = 0;

	return rump___sysimpl_clock_gettime50(clock_id, tp);
}

int
rk_getsockname(int arg1, int arg2)
{
	int shdmem_id, ret, fdes;
	static int old_shdmem_id = 0;
	static vaddr_t buf = 0;
	struct sockaddr *asa;
	socklen_t *alen;
	vaddr_t tmp;

	fdes      = arg1;
	shdmem_id = arg2;

	/* TODO, make this a function */
	if (old_shdmem_id != shdmem_id) {
		old_shdmem_id = shdmem_id;
		buf = rk_thdcalldata_shm_map(shdmem_id);
		assert(buf);
	}

	assert(buf && shdmem_id > 0 && (shdmem_id == old_shdmem_id));

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
	static int old_shdmem_id = 0;
	static vaddr_t buf = 0;
	struct sockaddr *asa;
	socklen_t *alen;
	vaddr_t tmp;

	fdes      = arg1;
	shdmem_id = arg2;

	/* TODO, make this a function */
	if (old_shdmem_id != shdmem_id) {
		old_shdmem_id = shdmem_id;
		buf = rk_thdcalldata_shm_map(shdmem_id);
		assert(buf);
	}

	assert(buf && shdmem_id > 0 && (shdmem_id == old_shdmem_id));

	tmp  = buf;
	asa  = (struct sockaddr *)tmp;
	tmp += sizeof(struct sockaddr);
	alen = (socklen_t *)tmp;

	return rump___sysimpl_getpeername(fdes, asa, alen);
}

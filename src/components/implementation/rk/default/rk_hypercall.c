#include <cos_kernel_api.h>
#include <cos_types.h>
#include <cringbuf.h>
#include <sinv_calls.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <rumpcalls.h>
#include <vk_types.h>
#include <llprint.h>
#include <rk.h>
#include <memmgr.h>
#include <fcntl.h>
#include <rk_thddata.h>

int     rump___sysimpl_socket30(int, int, int);
int     rump___sysimpl_bind(int, const struct sockaddr *, socklen_t);
ssize_t rump___sysimpl_recvfrom(int, void *, size_t, int, struct sockaddr *, socklen_t *);
ssize_t rump___sysimpl_sendto(int, const void *, size_t, int, const struct sockaddr *, socklen_t);
int     rump___sysimpl_setsockopt(int, int, int, const void *, socklen_t);
int     rump___sysimpl_getsockopt(int, int, int, void *, socklen_t *);
int     rump___sysimpl_listen(int, int);
int     rump___sysimpl_connect(int, const struct sockaddr *, socklen_t);
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
int     rump___sysimpl_close(int);
ssize_t rump___sysimpl_writev(int, const struct iovec *, int);
int     rump___sysimpl_fcntl(int, int, void *);

#define __SOCKADDR_NOLEN(s) do { if ((s)->sa_family >= (1 << 8)) (s)->sa_family >>= 8; } while (0)

/* These synchronous invocations involve calls to and from a RumpKernel */

static vaddr_t
rk_thdcalldata_shm_map(cbuf_t id)
{
	return rk_thddata_shm_get(&id);
}

int
rk_init(int id)
{
	return rk_thddata_shm_get((cbuf_t *)&id);
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
	static vaddr_t buf = 0;
	vaddr_t tmp;
	fd_set *in = NULL, *ou = NULL, *ex = NULL;
	struct timeval *tv = NULL;
	int *null_array;

	buf = rk_thdcalldata_shm_map(shdmem_id);
	assert(buf && shdmem_id > 0);

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
	vaddr_t buff = 0;
	vaddr_t tmp;
	struct sockaddr *name;
	socklen_t *anamelen;

	buff = rk_thdcalldata_shm_map(shdmem_id);
	assert(buff && shdmem_id > 0);

	tmp = buff;
	name = (struct sockaddr *)tmp;
	tmp += sizeof(struct sockaddr);
	anamelen = (socklen_t *)tmp;

	ret = rump___sysimpl_accept(s, name, anamelen);
	__SOCKADDR_NOLEN(name);

	return ret;
}

int
get_boot_done(void) {
	return 1;
}

int
rk_socket(int domain, int type, int protocol)
{
	int ret = 0, ret2 = 0;

	ret = rump___sysimpl_socket30(domain, type, protocol);
	if (ret >= 0) ret2 = rk_thddata_sock_set(ret);
	assert(ret2 >= 0);

	return ret;
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
	const void *buf = NULL;

	shdmem_id = arg1;
	flags     = arg2;
	mode    = (mode_t)arg3;

	buf = (void *)rk_thdcalldata_shm_map(shdmem_id);
	assert(buf && shdmem_id > 0);

	path = buf;

	flags = rk_open_flag(flags);
	return rump___sysimpl_open(path, flags, mode);
}

int
rk_close(int fd)
{
	if (fd == 0 || fd == 1 || fd == 2) return 0;

	return rump___sysimpl_close(fd);
}

int
rk_unlink(int arg1)
{
	int shdmem_id, ret;
	const char *path;
	const void *buf = NULL;

	shdmem_id = arg1;

	buf = (void *)rk_thdcalldata_shm_map(shdmem_id);
	assert(buf && shdmem_id > 0);

	path = buf;
	return rump___sysimpl_unlink(path);
}

int
rk_connect(int sockfd, int shmid, socklen_t addrlen)
{
	struct sockaddr *addr = NULL;
	int ret = 0;

	addr = (struct sockaddr *)rk_thdcalldata_shm_map(shmid);
	assert(addr && shmid > 0);

	__SOCKADDR_NOLEN(addr);
	ret = rump___sysimpl_connect(sockfd, addr, addrlen);

	return ret;
}

int
rk_bind(int sockfd, int shmid, socklen_t addrlen)
{
	struct sockaddr *addr = NULL;
	int ret = 0;

	addr = (struct sockaddr *)rk_thdcalldata_shm_map(shmid);
	assert(addr && shmid > 0);

	__SOCKADDR_NOLEN(addr);
	ret = rump___sysimpl_bind(sockfd, addr, addrlen);

	return ret;
}


#define RK_MSG_OOB         0x0001          /* process out-of-band data */
#define RK_MSG_PEEK        0x0002          /* peek at incoming message */
#define RK_MSG_DONTROUTE   0x0004          /* send without using routing tables */
#define RK_MSG_EOR         0x0008          /* data completes record */
#define RK_MSG_TRUNC       0x0010          /* data discarded before delivery */
#define RK_MSG_CTRUNC      0x0020          /* control data lost before delivery */
#define RK_MSG_WAITALL     0x0040          /* wait for full request or error */
#define RK_MSG_DONTWAIT    0x0080          /* this message should be nonblocking */
#define RK_MSG_NOSIGNAL    0x0400          /* do not generate SIGPIPE on EOF */
#define RK_MSG_CMSG_CLOEXEC 0x0800         /* close on exec receiving fd */

#define MUSL_MSG_OOB       0x0001
#define MUSL_MSG_PEEK      0x0002
#define MUSL_MSG_DONTROUTE 0x0004
#define MUSL_MSG_CTRUNC    0x0008
#define MUSL_MSG_TRUNC     0x0020
#define MUSL_MSG_DONTWAIT  0x0040
#define MUSL_MSG_EOR       0x0080
#define MUSL_MSG_WAITALL   0x0100
#define MUSL_MSG_NOSIGNAL  0x4000
#define MUSL_MSG_CMSG_CLOEXEC 0x40000000

int
rk_send_rcv_flags(int musl_flag)
{
	int rk_flag = 0;

	if (musl_flag & MUSL_MSG_OOB) rk_flag |= RK_MSG_OOB;
	if (musl_flag & MUSL_MSG_DONTROUTE) rk_flag |= RK_MSG_DONTROUTE;
	if (musl_flag & MUSL_MSG_PEEK) rk_flag |= RK_MSG_PEEK;
	if (musl_flag & MUSL_MSG_CTRUNC) rk_flag |= RK_MSG_CTRUNC;
	if (musl_flag & MUSL_MSG_EOR) rk_flag |= RK_MSG_EOR;
	if (musl_flag & MUSL_MSG_TRUNC) rk_flag |= RK_MSG_TRUNC;
	if (musl_flag & MUSL_MSG_WAITALL) rk_flag |= RK_MSG_WAITALL;
	if (musl_flag & MUSL_MSG_DONTWAIT) rk_flag |= RK_MSG_DONTWAIT;
	if (musl_flag & MUSL_MSG_NOSIGNAL) rk_flag |= RK_MSG_NOSIGNAL;
	if (musl_flag & MUSL_MSG_CMSG_CLOEXEC) rk_flag |= RK_MSG_CMSG_CLOEXEC;
	/* NOTE: not all flags in musl are supported by RK! */

	return rk_flag;
}

ssize_t
rk_recvfrom(int sockfd, int len, int shmid)
{
	void *buf = NULL;
	vaddr_t shmaddr = 0;
	struct sockaddr *src_addr = NULL;
	socklen_t *addrlen = NULL;
	int flags = 0;
	ssize_t ret = 0;

	shmaddr = rk_thdcalldata_shm_map(shmid);
	assert(shmaddr && shmid > 0);

	addrlen = (socklen_t *)(shmaddr + 8);
	if (flags == MUSL_MSG_DONTWAIT) flags = 0;
	assert(flags == 0); /* TODO: check if a flag can be supported or not! not all are supported or compatible */
	if (*(int *)(shmaddr + 4) == 1) src_addr = (struct sockaddr *)(shmaddr + 12);
	buf = (void *)(shmaddr + 12 + sizeof(struct sockaddr_storage));

	ret = rump___sysimpl_recvfrom(sockfd, buf, len, flags, src_addr, addrlen);
	if (src_addr) __SOCKADDR_NOLEN(src_addr);

	return ret;
}

ssize_t
rk_sendto(int sockfd, int len, int shmid)
{
	vaddr_t shmaddr = 0;
	struct sockaddr *dest_addr = NULL;
	socklen_t addrlen = 0;
	void *buf = NULL;
	ssize_t ret;
	int flags;

	shmaddr = rk_thdcalldata_shm_map(shmid);
	assert(shmaddr && shmid > 0);

	flags = rk_send_rcv_flags(*(int *)shmaddr);
	addrlen = *(socklen_t *)(shmaddr + 4);
	dest_addr = (struct sockaddr *)(shmaddr + 8);
	buf = (void *)(shmaddr + 8 + sizeof(struct sockaddr_storage));
	__SOCKADDR_NOLEN(dest_addr);

	ret = rump___sysimpl_sendto(sockfd, (const void *)buf, len, flags, (const struct sockaddr *)dest_addr, addrlen);

	return ret;
}

#define RK_SOL_SOCKET     0xffff
#define RK_SO_REUSEADDR   0x0004

#define MUSL_SOL_SOCKET   1
#define MUSL_SO_REUSEADDR 2

int
rk_socklevel_get(int muslvl)
{
	switch(muslvl) {
	case MUSL_SOL_SOCKET: return RK_SOL_SOCKET;
	default: assert(0);
	}

	assert(0);

	return -1;
}

int
rk_sockoptname_get(int musloptnm)
{
	switch(musloptnm) {
	case MUSL_SO_REUSEADDR: return RK_SO_REUSEADDR;
	default: assert(0);
	}

	assert(0);

	return -1;
}

int
rk_setsockopt(int arg1, int arg2, int arg3)
{
	int sockfd, level, optname, shdmem_id, ret;
	void *optval = NULL;
	socklen_t optlen;

	sockfd     = (arg1 >> 16);
	level      = (arg1 << 16) >> 16;
	optname    = (arg2 >> 16);
	shdmem_id  = (arg2 << 16) >> 16;
	optlen     = arg3;

	optval = (void *)rk_thdcalldata_shm_map(shdmem_id);
	assert(optval && shdmem_id > 0);

	level = rk_socklevel_get(level);
	optname = rk_sockoptname_get(optname);

	return rump___sysimpl_setsockopt(sockfd, level, optname, (const void *)optval, optlen);
}

int
rk_getsockopt(int sockfd_shmid, int level, int optname)
{
	int sockfd = (sockfd_shmid >> 16), shdmem_id = (sockfd_shmid << 16) >> 16, ret;
	void *optval = NULL;
	socklen_t *optlen = NULL;


	optlen = (socklen_t *)rk_thdcalldata_shm_map(shdmem_id);
	assert(optlen);
	optval = (void *)((vaddr_t)optlen + 4);
	assert(optlen && optval && shdmem_id > 0);

	ret = rump___sysimpl_getsockopt(sockfd, level, optname, optval, optlen);

	return ret;
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
	const void *buf = NULL;
	size_t nbyte;

	fd        = arg1;
	shdmem_id = arg2;
	nbyte     = (size_t)arg3;

	buf = (void *)rk_thdcalldata_shm_map(shdmem_id);
	assert(buf && shdmem_id > 0);

	return (long)rump___sysimpl_write(fd, buf, nbyte);
}

#define MUSL_O_LARGEFILE 0100000

#define RK_F_GETFL         3               /* get file status flags */
#define RK_F_SETFL         4               /* set file status flags */

#define MUSL_F_GETFL  3
#define MUSL_F_SETFL  4

int
rk_fcntl(int fd, int cmd, int arg3)
{
	assert(cmd == F_SETFL);
	assert(arg3 == (MUSL_O_NONBLOCK | MUSL_O_LARGEFILE));

	arg3 = RK_O_NONBLOCK;

	return rump___sysimpl_fcntl(fd, cmd, arg3);
}

ssize_t
rk_writev(int fd, int iovcnt, int shmid)
{
	int shdmem_id, ret, i = 0;
	struct iovec *iobuf = NULL;
	void *tmpbuf = NULL;

	shdmem_id = shmid;

	iobuf = (struct iovec *)rk_thdcalldata_shm_map(shdmem_id);
	assert(iobuf && shdmem_id > 0);

	tmpbuf = ((void *)iobuf) + (sizeof(struct iovec) * iovcnt);
	for (i = 0; i < iovcnt; i++) {
		/* update shared memory addresses to my virtual addresses! */
		if (i) tmpbuf += iobuf[i-1].iov_len;
		iobuf[i].iov_base = tmpbuf;
	}

	return rump___sysimpl_writev(fd, (const struct iovec *)iobuf, iovcnt);
}

long
rk_read(int arg1, int arg2, int arg3)
{
	int fd, shdmem_id, ret;
	const void *buf = NULL;
	size_t nbyte;

	fd        = arg1;
	shdmem_id = arg2;
	nbyte     = (size_t)arg3;

	buf = (void *)rk_thdcalldata_shm_map(shdmem_id);
	assert(buf && shdmem_id > 0);

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
	struct timespec *tp = NULL;

	clock_id  = (clockid_t)arg1;
	shdmem_id = arg2;

	tp = (struct timespec *)rk_thdcalldata_shm_map(shdmem_id);
	assert(tp && shdmem_id > 0);

	/* Per process clock 2 is not supported, user real-time clock */
	if (clock_id == 2) clock_id = 0;

	return rump___sysimpl_clock_gettime50(clock_id, tp);
}

int
rk_getsockname(int arg1, int arg2)
{
	int shdmem_id, ret, fdes;
	vaddr_t buf = 0;
	struct sockaddr *asa;
	socklen_t *alen;
	vaddr_t tmp;

	fdes      = arg1;
	shdmem_id = arg2;

	buf = rk_thdcalldata_shm_map(shdmem_id);
	assert(buf && shdmem_id > 0);

	tmp  = buf;
	asa  = (struct sockaddr *)tmp;
	tmp += sizeof(struct sockaddr);
	alen = (socklen_t *)tmp;

	ret = rump___sysimpl_getsockname(fdes, asa, alen);
	__SOCKADDR_NOLEN(asa);

	return ret;
}

int
rk_getpeername(int arg1, int arg2)
{
	int shdmem_id, ret, fdes;
	vaddr_t buf = 0;
	struct sockaddr *asa;
	socklen_t *alen;
	vaddr_t tmp;

	fdes      = arg1;
	shdmem_id = arg2;

	buf = rk_thdcalldata_shm_map(shdmem_id);
	assert(buf && shdmem_id > 0);

	tmp  = buf;
	asa  = (struct sockaddr *)tmp;
	tmp += sizeof(struct sockaddr);
	alen = (socklen_t *)tmp;

	ret = rump___sysimpl_getpeername(fdes, asa, alen);
	__SOCKADDR_NOLEN(asa);

	return ret;
}

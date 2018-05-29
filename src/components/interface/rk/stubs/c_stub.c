#include <rk_inv.h>
#include <cos_types.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <llprint.h>
#include <rk.h>
#include <string.h>
#include "../capmgr/memmgr.h"
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <rk_socketcall_types.h>

static inline vaddr_t
rk_get_shm_callvaddr(cbuf_t *shmid)
{
	/* reentrant.. each thread will alloc a new shared region.. */
	static cbuf_t id_calldata[MAX_NUM_THREADS] = { 0 };
	static vaddr_t addr_calldata[MAX_NUM_THREADS] = { 0 };

	if (unlikely(id_calldata[cos_thdid()] == 0)) id_calldata[cos_thdid()] = memmgr_shared_page_alloc(&addr_calldata[cos_thdid()]);

	assert(id_calldata[cos_thdid()] && addr_calldata[cos_thdid()]);
	*shmid = id_calldata[cos_thdid()];

	return addr_calldata[cos_thdid()];
}

void *
rk_inv_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off)
{
	void *ret=0;

	if (addr != NULL) {
		printc("parameter void *addr is not supported!\n");
		errno = ENOTSUP;
		return MAP_FAILED;
	}
	if (fd != -1) {
		printc("WARNING, file mapping is not supported, ignoring file\n");
	}

	printc("getting %d number of pages\n", len / PAGE_SIZE);
	addr = (void *)memmgr_heap_page_allocn(len / PAGE_SIZE);
	printc("addr: %p\n", addr);
	if (!addr){
		ret = (void *) -1;
	} else {
		ret = addr;
	}

	if (ret == (void *)-1) {  /* return value comes from man page */
		printc("mmap() failed!\n");
		/* This is a best guess about what went wrong */
		errno = ENOMEM;
	}
	return ret;
}

ssize_t
rk_inv_write(int fd, const void *buf, size_t nbyte)
{
	cbuf_t shmid = 0;
	vaddr_t shmaddr = NULL;

	shmaddr = rk_get_shm_callvaddr(&shmid);

	memcpy((void *)shmaddr, buf, nbyte);

	return rk_write(fd, shmid, nbyte);
}

int
rk_inv_close(int fd)
{
	return rk_close(fd);
}

int
rk_inv_unlink(const char *path)
{
	cbuf_t shmid = 0;
	vaddr_t shmaddr = NULL;

	shmaddr = rk_get_shm_callvaddr(&shmid);

	memcpy((void *)shmaddr, path, 100);

	return rk_unlink(shmid);
}

int
rk_inv_ftruncate(int fd, off_t length)
{ return rk_ftruncate(fd, length); }

ssize_t
rk_inv_read(int fd, void *buf, size_t nbyte)
{
	long ret;
	cbuf_t shmid = 0;
	vaddr_t shmaddr = NULL;

	shmaddr = rk_get_shm_callvaddr(&shmid);

	assert(nbyte <= PAGE_SIZE);
	ret = rk_read(fd, shmid, nbyte);

	assert(ret <= PAGE_SIZE);
	memcpy((void *)buf, (void *)shmaddr, ret);

	return ret;
}

int
rk_inv_open(const char *path, int flags, mode_t mode)
{
	cbuf_t shmid = 0;
	vaddr_t shmaddr = NULL;

	shmaddr = rk_get_shm_callvaddr(&shmid);

	memcpy((void *)shmaddr, path, 100);

	return rk_open(shmid, flags, mode);
}

int
rk_inv_clock_gettime(clockid_t clock_id, struct timespec *tp)
{
	cbuf_t shmid = 0;
	vaddr_t shmaddr = NULL;
	int ret;

	shmaddr = rk_get_shm_callvaddr(&shmid);

	memcpy((void * __restrict__)shmaddr, tp, sizeof(struct timespec));

	ret = rk_clock_gettime(clock_id, shmid);
	assert(!ret);

	/* Copy shdmem back into tp */
	memcpy(tp, (void *)shmaddr, sizeof(struct timespec));

	return ret;
}

static void
__set_valid(int *null_array)
{
	int i, num = 4; /* Because select takes 4 pointers */
	for (i = 0 ; i < num ; i++) *(null_array + i) = 1;
}

int
rk_inv_select(int nd, fd_set *in, fd_set *ou, fd_set *ex, struct timeval *tv)
{
	cbuf_t shmid = 0;
	vaddr_t shmaddr = NULL;
	int ret;
	int *null_array;
	vaddr_t tmp;

	shmaddr = rk_get_shm_callvaddr(&shmid);

	/* XXX This is A LOT of copying. Optimize me*/

	/*
	 * Select can pass in null to the pointer arguments, because we assign the pointers
	 * to shared memory we need something to keep track of which of the values were null
	 */

	null_array = (void *)shmaddr;
	__set_valid(null_array);
	tmp = shmaddr + (sizeof(int) * 4);

	if (in) memcpy((void *)tmp, in, sizeof(fd_set));
	else null_array[0] = 0;
	tmp += sizeof(fd_set);

	if (ou) memcpy((void *)tmp, ou, sizeof(fd_set));
	else null_array[1] = 0;
	tmp += sizeof(fd_set);

	if (ex) memcpy((void *)tmp, ex, sizeof(fd_set));
	else null_array[2] = 0;
	tmp += sizeof(fd_set);

	if (tv) memcpy((void *)tmp, tv, sizeof(struct timeval));
	else null_array[3] = 0;

	ret = rk_select(nd, shmid);

	tmp = shmaddr + (sizeof(int) * 4);
	if(in) memcpy(in, (void *)tmp, sizeof(fd_set));
	tmp += sizeof(fd_set);
	if(ou) memcpy(ou, (void *)tmp, sizeof(fd_set));
	tmp += sizeof(fd_set);
	if(ex) memcpy(ex, (void *)tmp, sizeof(fd_set));
	tmp += sizeof(fd_set);
	if(tv) memcpy(tv, (void *)tmp, sizeof(struct timeval));

	return ret;
}

int
rk_inv_socketcall(int call, unsigned long *args)
{
	cbuf_t shmid = 0;
	vaddr_t shmaddr = NULL;
	int ret = -1;

	shmaddr = rk_get_shm_callvaddr(&shmid);
	/*
	 * Set and unset by sendto and recvfrom to ensure that only 1 thread
	 * is sending and recieving packets. This means that we will never have
	 * have more than 1 packet in the send or recv shdmem page at a given time
	 */

	switch (call) {
		case SOCKETCALL_SOCKET: { /* socket */
			int domain, type, protocol;

			domain     = *args;
			type       = *(args + 1);
			protocol   = *(args + 2);

			ret = rk_socket(domain, type, protocol);

			break;
		}
		case SOCKETCALL_CONNECT: {
			int sockfd = *args;
			struct sockaddr *addr = (struct sockaddr *)*(args + 1), *saddr = (struct sockaddr *)shmaddr;
			socklen_t addrlen = (socklen_t)*(args + 2);

			memcpy(saddr, addr, addrlen);

			ret = rk_connect(sockfd, shmid, addrlen);

			break;
		}
		case SOCKETCALL_BIND: { /* bind */
			int sockfd;
			void *addr;
			u32_t addrlen;

			sockfd  = *args;
			addr    = (void *)*(args + 1);
			addrlen = *(args + 2);

			memcpy((void *)shmaddr, addr, addrlen);
			ret = rk_bind(sockfd, shmid, addrlen);

			break;
		}
		case SOCKETCALL_LISTEN: { /* listen */
			int s, backlog;

			s       = *args;
			backlog = *(args + 1);

			ret = rk_listen(s, backlog);

			break;
		}
		case SOCKETCALL_ACCEPT: { /* accept */
			int s;
			struct sockaddr *addr;
			socklen_t *addrlen;
			vaddr_t tmp;

			s       = *args;
			addr    = (struct sockaddr *)*(args + 1);
			addrlen = (socklen_t *)*(args + 2);

			/* Copy into shdmem */
			tmp = shmaddr;
			memcpy((void *)tmp, addr, sizeof(struct sockaddr));
			tmp += sizeof(struct sockaddr);
			memcpy((void *)tmp, addrlen, sizeof(vaddr_t));

			ret = rk_accept(s, shmid);

			/* Copy out of shdmem */
			tmp = shmaddr;
			memcpy(addr, (void *)tmp, sizeof(struct sockaddr));
			tmp += sizeof(struct sockaddr);
			memcpy(addrlen, (void *)tmp, sizeof(vaddr_t));

			break;
		}
		case SOCKETCALL_GETSOCKNAME: { /* getsockname */
			int fdes;
			struct sockaddr *asa;
			socklen_t *alen;
			vaddr_t tmp;

			fdes = *args;
			asa  = (struct sockaddr *)*(args + 1);
			alen = (socklen_t *)*(args + 2);

			/* Copy into shdmem */
			tmp = shmaddr;
			memcpy((void *)tmp, asa, sizeof(struct sockaddr));
			tmp += sizeof(struct sockaddr);
			memcpy((void *)tmp, alen, sizeof(socklen_t));

			ret = rk_getsockname(fdes, shmid);

			/* Copy out of shdmem */
			tmp = shmaddr;
			memcpy(asa, (void *)tmp, sizeof(struct sockaddr));
			tmp += sizeof(struct sockaddr);
			memcpy(alen, (void *)tmp, sizeof(socklen_t));

			break;
		}
		case SOCKETCALL_GETPEERNAME: { /* getpeername */
			int fdes;
			struct sockaddr *asa;
			socklen_t *alen;
			vaddr_t tmp;

			fdes = *args;
			asa  = (struct sockaddr *)*(args + 1);
			alen = (socklen_t *)*(args + 2);

			/* Copy into shdmem */
			tmp = shmaddr;
			memcpy((void *)tmp, asa, sizeof(struct sockaddr));
			tmp += sizeof(struct sockaddr);
			memcpy((void *)tmp, alen, sizeof(socklen_t));

			ret = rk_getpeername(fdes, shmid);

			/* Copy out of shdmem */
			tmp = shmaddr;
			memcpy(asa, (void *)tmp, sizeof(struct sockaddr));
			tmp += sizeof(struct sockaddr);
			memcpy(alen, (void *)tmp, sizeof(socklen_t));

			break;
		}
		case SOCKETCALL_SENDTO: { /* sendto */
			int fd, flags;
			vaddr_t shmaddr_tmp;
			const void *buff;
			void *shdmem_buff;
			size_t len;
			const struct sockaddr *addr;
			struct sockaddr *shdmem_sockaddr;
			socklen_t addrlen;

			fd      = (int)*args;
			buff    = (const void *)*(args + 1);
			len     = (size_t)*(args + 2);
			flags   = (int)*(args + 3);
			addr    = (const struct sockaddr *)*(args + 4);
			addrlen = (socklen_t)*(args + 5);

			shmaddr_tmp = shmaddr;
			shdmem_buff = (void *)shmaddr_tmp;
			memcpy(shdmem_buff, buff, len);
			shmaddr_tmp += len;

			shdmem_sockaddr = (struct sockaddr*)shmaddr_tmp;
			memcpy(shdmem_sockaddr, addr, addrlen);

			assert(fd <= 0xFFFF);
			assert(shmid <= 0xFFFF);
			assert(len <= 0xFFFF);
			assert(flags <= 0xFFFF);
			assert(shmid <= (int)0xFFFF);
			assert(addrlen <= (int)0xFFFF);

			ret = rk_sendto((fd << 16) | shmid, (len << 16) | flags,
					(shmid << 16) | addrlen);

			break;
		}
		case SOCKETCALL_RECVFROM: { /* recvfrom */
			int s, flags;
			vaddr_t shmaddr_tmp;
			void *buff;
			size_t len;
			struct sockaddr *from_addr;
			u32_t *from_addr_len_ptr;

			s                 = *args;
			buff              = (void *)*(args + 1);
			len               = *(args + 2);
			flags             = *(args + 3);
			from_addr         = (struct sockaddr *)*(args + 4);
			from_addr_len_ptr = (u32_t *)*(args + 5);

			assert(s <= 0xFFFF);
			assert(shmid <= 0xFFFF);
			assert(len <= 0xFFFF);
			assert(flags <= 0xFFFF);
			assert(shmid <= 0xFFFF);
			assert((*from_addr_len_ptr) <= 0xFFFF);

			ret = rk_recvfrom((s << 16) | shmid, (len << 16) | flags,
					  (shmid << 16) | (*from_addr_len_ptr));

			/* TODO, put this in a function */
			/* Copy buffer back to its original value*/
			shmaddr_tmp = shmaddr;
			memcpy(buff, (const void * __restrict__)shmaddr_tmp, ret);
			shmaddr_tmp += len; /* Add overall length of buffer */

			/* Set from_addr_len_ptr pointer to be shared memory at right offset */
			*from_addr_len_ptr = *(u32_t *)shmaddr_tmp;
			shmaddr_tmp += sizeof(u32_t *);

			/* Copy from_addr to be shared memory at right offset */
			memcpy(from_addr, (const void * __restrict__)shmaddr_tmp, *from_addr_len_ptr);

			break;
		}
		case SOCKETCALL_SETSOCKOPT: { /* setsockopt */
			int sockfd, level, optname;
			const void *optval;
			socklen_t optlen;

			sockfd            = (int)*args;
			level		  = (int)*(args + 1);
			optname           = (int)*(args + 2);
			optval            = (const void *)*(args + 3);
			optlen            = (socklen_t)*(args + 4);

			memcpy((void *)shmaddr, optval, optlen);

			ret = rk_setsockopt((sockfd << 16) | level, (optname << 16) | shmid, optlen);

			memcpy((void *)optval, (void *)shmaddr, optlen);

			break;
		}
		case SOCKETCALL_GETSOCKOPT: {
			/* int getsockopt(int sockfd, int level, int optname, void *optval, socklen_t *optlen); */
			int sockfd = (int)*args, level = (int)*(args + 1), optname = (int)*(args + 2);
			void *optval = (void *)*(args + 3), *roptval = NULL;
			socklen_t *optlen = (socklen_t *)*(args + 4), *roptlen = NULL;

			/* output/return values on shared memory */
			roptlen = (socklen_t *)shmaddr;
			/* holds input len */
			*roptlen = *optlen;
			roptval = (void *)(shmaddr + 4);

			assert(sockfd < (1<<16) && shmid < (1<<16));
			ret = rk_getsockopt(sockfd << 16 | shmid, level, optname);
			/* copy from shared memory to user passed addresses */
			*optlen = *roptlen;
			memcpy(optval, roptval, *optlen);

			break;
		}
		default: {
			printc("%s, ERROR, unimplemented socket call: %d\n", __func__, call);
			assert(0);
		}
	}

	return ret;
}

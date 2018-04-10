#include <rk_inv_api.h>
#include <cos_types.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <posix.h>
#include <rumpcalls.h>
#include <vk_types.h>
#include <llprint.h>
#include <rk.h>
#include <string.h>
#include <memmgr.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

extern int spdid;

void *
rk_inv_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off)
{
	void *ret=0;
	printc("%s\n", __func__);

	if (addr != NULL) {
		printc("parameter void *addr is not supported!\n");
		errno = ENOTSUP;
		return MAP_FAILED;
	}
	if (fd != -1) {
		printc("file mapping is not supported!\n");
		errno = ENOTSUP;
		return MAP_FAILED;
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

long
rk_inv_write(int fd, const void *buf, size_t nbyte)
{
	int shdmem_id = -1;
	vaddr_t shdmem_addr = 0;

	shdmem_id = memmgr_shared_page_alloc(&shdmem_addr);
        assert(shdmem_id > -1 && shdmem_addr > 0);

        memcpy((void * __restrict__)shdmem_addr, buf, nbyte);

	printc("UNTESTED, %s, fd: %d, shdmem_id: %d, nbyte: %d\n", __func__, fd, shdmem_id, nbyte);

	return rk_write(fd, shdmem_id, nbyte);
}

int
rk_inv_clock_gettime(clockid_t clock_id, struct timespec *tp)
{
	static int shdmem_id = -1;
	static vaddr_t shdmem_addr = 0;
	int ret;

	if (shdmem_id == -1) {
		shdmem_id = memmgr_shared_page_alloc(&shdmem_addr);
	}
        assert(shdmem_id > -1 && shdmem_addr > 0);

        memcpy((void * __restrict__)shdmem_addr, tp, sizeof(struct timespec));

	ret = rk_clock_gettime(clock_id, shdmem_id);
	assert(!ret);

	/* Copy shdmem back into tp */
        memcpy(tp, shdmem_addr, sizeof(struct timespec));

	return ret;
}

int
rk_inv_select(int nd, fd_set *in, fd_set *ou, fd_set *ex, struct timeval *tv)
{
	static int shdmem_id = -1;
	static vaddr_t shdmem_addr = 0;
	int ret;

	if (shdmem_id == -1) {
		shdmem_id = memmgr_shared_page_alloc(&shdmem_addr);
	}
        assert(shdmem_id > -1 && shdmem_addr > 0);

	/* XXX This is A LOT of copying. Think about how this can be optimized */


	if (in) memcpy(shdmem_addr, in, sizeof(fd_set));
	shdmem_addr += sizeof(fd_set);
	if (ou) memcpy(shdmem_addr, ou, sizeof(fd_set));
	shdmem_addr += sizeof(fd_set);
	if (ex) memcpy(shdmem_addr, ex, sizeof(fd_set));
	shdmem_addr += sizeof(fd_set);
	if (tv) memcpy(shdmem_addr, tv, sizeof(struct timeval));

	ret = rk_select(nd, shdmem_id);

	if(tv) memcpy(tv, shdmem_addr, sizeof(struct timeval));
	shdmem_addr -= sizeof(fd_set);
	if(ex) memcpy(ex, shdmem_addr, sizeof(fd_set));
	shdmem_addr -= sizeof(fd_set);
	if(ou) memcpy(ou, shdmem_addr, sizeof(fd_set));
	shdmem_addr -= sizeof(fd_set);
	if(in) memcpy(in, shdmem_addr, sizeof(fd_set));

	return ret;
}

int
rk_inv_op1(void)
{ return test_entry(0, 0, 0, 0); }

int
rk_inv_get_boot_done(void)
{ return get_boot_done(); }

int
rk_inv_socket(int domain, int type, int protocol)
{ return rk_socket(domain, type, protocol); }

int
rk_inv_bind(int sockfd, int shdmem_id, socklen_t addrlen)
{ return rk_bind(sockfd, shdmem_id, addrlen); }

int
rk_inv_listen(int s, int backlog)
{ return rk_listen(s, backlog); }


ssize_t
rk_inv_recvfrom(int s, int buff_shdmem_id, size_t len, int flags, int from_shdmem_id, int fromlenaddr_shdmem_id)
{
	assert(s <= 0xFFFF);
	assert(buff_shdmem_id <= 0xFFFF);
	assert(len <= 0xFFFF);
	assert(flags <= 0xFFFF);
	assert(from_shdmem_id <= 0xFFFF);
	assert(fromlenaddr_shdmem_id <= 0xFFFF);

	return (ssize_t)rk_recvfrom((s << 16) | buff_shdmem_id, (len << 16) | flags,
			(from_shdmem_id << 16) | fromlenaddr_shdmem_id);
}

ssize_t
rk_inv_sendto(int sockfd, int buff_shdmem_id, size_t len, int flags, int addr_shdmem_id, socklen_t addrlen)
{
	assert(sockfd <= 0xFFFF);
	assert(buff_shdmem_id <= 0xFFFF);
	assert(len <= 0xFFFF);
	assert(flags <= 0xFFFF);
	assert(addr_shdmem_id <= (int)0xFFFF);
	assert(addrlen <= (int)0xFFFF);

	return (ssize_t)rk_sendto((sockfd << 16) | buff_shdmem_id, (len << 16) | flags,
				  (addr_shdmem_id << 16) | addrlen);
}

int
rk_inv_setsockopt(int sockfd, int level, int optname, int shdmem_id, socklen_t optlen)
{ return rk_setsockopt((sockfd << 16) | level, (optname << 16) | shdmem_id, optlen); }

int
rk_socketcall(int call, unsigned long *args)
{
        int ret = -1;
	/*
	 * Set and unset by sendto and recvfrom to ensure that only 1 thread
	 * is sending and recieving packets. This means that we will never have
	 * have more than 1 packet in the send or recv shdmem page at a given time
	 */
	static int canSend = 0;

        switch (call) {
		case 1: { /* socket */
                        int domain, type, protocol;

                        domain     = *args;
                        type       = *(args + 1);
                        protocol   = *(args + 2);
			printc("domain: %d, type: %d, protocol: %d\n",
					domain, type, protocol);
                        ret = rk_inv_socket(domain, type, protocol);
			printc("ret from rk_inv_socket: %d\n", ret);

                        break;
                }
                case 2: { /* bind */
                        int sockfd, shdmem_id;
                        vaddr_t shdmem_addr;
                        void *addr;
                        u32_t addrlen;

                        sockfd  = *args;
                        addr    = (void *)*(args + 1);
                        addrlen = *(args + 2);

                        /*
                         * Do stupid shared memory for now
                         * allocate a page for each bind addr
                         * don't deallocate. #memLeaksEverywhere
			 * We don't have to fix this for now as we only have 1 bind
                         */

			/* TODO make this a function */
                        shdmem_id = memmgr_shared_page_alloc(&shdmem_addr);
                        assert(shdmem_id > -1 && shdmem_addr > 0);

                        memcpy((void * __restrict__)shdmem_addr, addr, addrlen);
                        ret = rk_inv_bind(sockfd, shdmem_id, addrlen);

                        break;
                }
		case 4: { /* listen */
			int s, backlog;

                        s       = *args;
                        backlog = *(args + 1);

			printc("rk listen\n");
			ret = rk_inv_listen(s, backlog);
			printc("ret from listen: %d\n", ret);

			break;
		}
		case 11: { /* sendto */
			int fd, flags;
			static int shdmem_id = -1;
			static vaddr_t shdmem_addr = 0;
			vaddr_t shdmem_addr_tmp;
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

			/* TODO make this a function */
			if (shdmem_id < 0 && !shdmem_addr) {
				shdmem_id = memmgr_shared_page_alloc(&shdmem_addr);
			}

                        assert(shdmem_id > -1 && shdmem_addr > 0);

			assert(canSend == 1);
			canSend = 0;

			shdmem_addr_tmp = shdmem_addr;
			shdmem_buff = (void *)shdmem_addr_tmp;
			memcpy(shdmem_buff, buff, len);
			shdmem_addr_tmp += len;

			shdmem_sockaddr = (struct sockaddr*)shdmem_addr_tmp;
			memcpy(shdmem_sockaddr, addr, addrlen);

			ret = (int)rk_inv_sendto(fd, shdmem_id, len, flags,
				shdmem_id, addrlen);

			break;
		}
		case 12: { /* recvfrom */
                        int s, flags;
			static int shdmem_id = -1;
			static vaddr_t shdmem_addr = 0;
			vaddr_t shdmem_addr_tmp;
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

			/* TODO make this a function */
			if (shdmem_id < 0 && !shdmem_addr) {
				shdmem_id = memmgr_shared_page_alloc(&shdmem_addr);
			}

                        assert(shdmem_id > -1 && shdmem_addr > 0);

			assert(canSend == 0);
			canSend = 1;

                        ret = (int)rk_inv_recvfrom(s, shdmem_id, len, flags,
                                shdmem_id, *from_addr_len_ptr);

                        /* TODO, put this in a function */
                        /* Copy buffer back to its original value*/
			shdmem_addr_tmp = shdmem_addr;
                        memcpy(buff, (const void * __restrict__)shdmem_addr_tmp, ret);
                        shdmem_addr_tmp += len; /* Add overall length of buffer */

                        /* Set from_addr_len_ptr pointer to be shared memory at right offset */
                        *from_addr_len_ptr = *(u32_t *)shdmem_addr_tmp;
                        shdmem_addr_tmp += sizeof(u32_t *);

                        /* Copy from_addr to be shared memory at right offset */
                        memcpy(from_addr, (const void * __restrict__)shdmem_addr_tmp, *from_addr_len_ptr);

                        break;
                }
		case 14: { /* setsockopt */
			int sockfd, level, optname;
			static int shdmem_id = -1;
			const void *optval;
			static vaddr_t shdmem_addr = 0;
			socklen_t optlen;

                        sockfd            = (int)*args;
                        level		  = (int)*(args + 1);
                        optname           = (int)*(args + 2);
                        optval            = (const void *)*(args + 3);
                        optlen            = (socklen_t)*(args + 4);

			printc("setsockopt in rk socket, sockfd: %d, level: %d, optname: %d, optval: %p, optlen: %u\n", sockfd, level, optname, optval, optlen);

			/* TODO make this a function */
			if (shdmem_id < 0 && !shdmem_addr) {
				shdmem_id = memmgr_shared_page_alloc(&shdmem_addr);
			}

                        assert(shdmem_id > -1 && shdmem_addr > 0);

			printc("%s, shdmem_addr: %p, shdmem_id: %d\n",
				__func__, shdmem_addr, shdmem_id);
			printc("copying %p into %p for len: %u\n", optval, shdmem_addr, optlen);
			memcpy(shdmem_addr, optval, optlen);

			ret = (int)rk_inv_setsockopt(sockfd, level, optname, shdmem_id, optlen);

			printc("copying %p back into %p for len: %u\n", shdmem_addr,
				optval, optlen);
			memcpy(optval, shdmem_addr, optlen);

			printc("ret from rk_inv_setsockopt: %d\n", ret);

			break;
		}
                default: {
			printc("%s, ERROR, unimplemented socket call: %d\n", __func__, call);
                        assert(0);
		}
        }

        return ret;
}


int
rk_libcmod_init(void)
{
	assert(spdid != 0);

	posix_syscall_override((cos_syscall_t)rk_socketcall, __NR_socketcall);
	posix_syscall_override((cos_syscall_t)rk_inv_mmap, __NR_mmap);
	posix_syscall_override((cos_syscall_t)rk_inv_mmap, __NR_mmap2);
	posix_syscall_override((cos_syscall_t)rk_inv_write, __NR_write);
	posix_syscall_override((cos_syscall_t)rk_inv_clock_gettime, __NR_clock_gettime);
	posix_syscall_override((cos_syscall_t)rk_inv_select, __NR__newselect);

	return 0;
}

#include <cos_kernel_api.h>
#include <cos_types.h>
#include <cringbuf.h>
#include <sinv_calls.h>
#include <shdmem.h>
#include <sys/socket.h>
#include <rumpcalls.h>
#include <vk_types.h>
#include <llprint.h>
#include <rk.h>

int rump___sysimpl_socket30(int, int, int);
int rump___sysimpl_bind(int, const struct sockaddr *, socklen_t);
ssize_t rump___sysimpl_recvfrom(int, void *, size_t, int, struct sockaddr *, socklen_t *);
ssize_t rump___sysimpl_sendto(int, const void *, size_t, int, const struct sockaddr *, socklen_t);

/* These synchronous invocations involve calls to and from a RumpKernel */
//extern struct cringbuf *vmrb;
/* TODO when rumpbooter is its own interface, have this as an exported symbol */
struct cringbuf *vmrb = NULL;

int
test_entry(int arg1, int arg2, int arg3, int arg4)
{
        int ret = 0;

        printc("\n*** KERNEL COMPONENT ***\n \tArguments: %d, %d, %d, %d\n", arg1, arg2, arg3, arg4);
        printc("spdid: %d\n", cos_spdid_get());
        printc("*** KERNEL COMPONENT RETURNING ***\n\n");

        return ret;
}

int
test_fs(int arg1, int arg2, int arg3, int arg4)
{
        int ret = 0;

        printc("\n*** KERNEL COMPONENT ***\n \tArguments: %d, %d, %d, %d\n", arg1, arg2, arg3, arg4);
        printc("spdid: %d\n", cos_spdid_get());

        /* FS Test */
        printc("Running paws test: VM%d\n", cos_spdid_get());
//      paws_tests();

        printc("*** KERNEL COMPONENT RETURNING ***\n\n");

        return ret;

}

int
test_shdmem(int shm_id, int arg2, int arg3, int arg4)
{
	/* TODO Retest after getting all componets booting independently with cos linker */
	//int my_id;
	//vaddr_t my_page;

	///* Calling from user component into kernel component */
	//assert(!cos_spdid_get());

	///* Map in shared page */
	//my_id = shm_map(cos_spdid_get(), shm_id);
	//assert(my_id == shm_id);

	///* Get our vaddr for this shm_id */
	//my_page = shm_get_vaddr(cos_spdid_get(), my_id);
	//assert(my_page);

	//printc("Kernel Component shared mem vaddr: %p\n", (void *)my_page);
	//printc("Reading from page: %c\n", *(char *)my_page);
	//printc("Writing 'b' to page + 1\n");
	//*((char *)my_page + 1) = 'b';

	return 0;
}

int
get_boot_done(void) {
	return 1;
}

int
rk_socket(int domain, int type, int protocol)
{ return rump___sysimpl_socket30(domain, type, protocol); }

int
rk_bind(int sockfd, int shdmem_id, socklen_t addrlen)
{
	const struct sockaddr *addr = NULL;
	shdmem_id = shm_map(cos_spdid_get(), shdmem_id);
	assert(shdmem_id > -1);
	addr = (const struct sockaddr *)shm_get_vaddr(cos_spdid_get(), shdmem_id);
	assert(addr);
	return rump___sysimpl_bind(sockfd, addr, addrlen);
}

ssize_t
rk_recvfrom(int s, int buff_shdmem_id, size_t len, int flags, int from_shdmem_id, int from_addr_len)
{
	static int shdmem_id = -1;
	static vaddr_t my_addr = 0;
	vaddr_t my_addr_tmp;
	void *buff;
	struct sockaddr *from;
	socklen_t *from_addr_len_ptr;

	if (shdmem_id == -1 && my_addr == 0) {
		shdmem_id = shm_map(cos_spdid_get(), buff_shdmem_id);
		my_addr = shm_get_vaddr(cos_spdid_get(), shdmem_id);
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
rk_sendto(int sockfd, int buff_shdmem_id, size_t len, int flags, int addr_shdmem_id, socklen_t addrlen)
{
	static int shdmem_id = -1;
	static const void *buff = 0;
	const struct sockaddr *addr;

	if (shdmem_id == -1 && buff == 0) {
		shdmem_id = shm_map(cos_spdid_get(), buff_shdmem_id);
		buff = (const void *)shm_get_vaddr(cos_spdid_get(), shdmem_id);
	}

	assert(shdmem_id > -1);
	assert(buff);
	assert(buff_shdmem_id == addr_shdmem_id && buff_shdmem_id == shdmem_id);

	addr = (const struct sockaddr *)(buff + len);
	assert(addr);

	return rump___sysimpl_sendto(sockfd, buff, len, flags, addr, addrlen);
}

int
rk_entry(int arg1, int arg2, int arg3, int arg4)
{
	int ret = 0;

	/* TODO, remove this function and instead have all function invocations auto-generated */
	switch(arg1) {
	case RK_INV_OP1: {
		ret = test_fs(arg2, arg3, arg4, 0);
		break;
	}
	case RK_INV_OP2: {
		ret = test_shdmem(arg2, arg3, arg4, 0);
		break;
	}
	case RK_GET_BOOT_DONE: {
		ret = get_boot_done();
		break;
	}
	case RK_SOCKET: {
		printc("RK socket\n");
		ret = rk_socket(arg2, arg3, arg4);
		break;
	}
	case RK_BIND: {
		printc("RK bind\n");
		ret = rk_bind(arg2, arg3, (socklen_t)arg4);
		break;
	}
	case RK_RECVFROM: {
		printc("RK recvfrom\n");
		int s, buff_shdmem_id, flags, from_shdmem_id, from_addr_len;
		size_t len;

		s = (arg2 >> 16);
		buff_shdmem_id = (arg2 << 16) >> 16;
		len = (arg3 >> 16);
		flags = (arg3 << 16) >> 16;
		from_shdmem_id = (arg4 >> 16);
		from_addr_len = (arg4 << 16) >> 16;

		ret = (int)rk_recvfrom(s, buff_shdmem_id, len, flags,
				from_shdmem_id, from_addr_len);
		break;
	}
	case RK_SENDTO: {
		printc("RK sendto\n");
		int sockfd, flags, buff_shdmem_id, addr_shdmem_id;
		size_t len;
		socklen_t addrlen;
		const struct sockaddr *addr;

		sockfd            = (arg2 >> 16);
		buff_shdmem_id    = (arg2 << 16) >> 16;
		len               = (arg3 >> 16);
		flags             = (arg3 << 16) >> 16;
		addr_shdmem_id    = (arg4 >> 16);
		addrlen           = (arg4 << 16) >> 16;

		ret = (int)rk_sendto(sockfd, buff_shdmem_id, len, flags, addr_shdmem_id, addrlen);
		break;
	}
	case RK_LOGDATA: {
		/* DEPRECATED */
		break;
	}
	default: assert(0);
	}

	return ret;
}

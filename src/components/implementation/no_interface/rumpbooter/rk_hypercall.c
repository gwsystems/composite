#include <cos_kernel_api.h>
#include <cos_types.h>
#include <cringbuf.h>
#include <sinv_calls.h>
#include <shdmem.h>
#include <rk_inv_api.h>
#include "rumpcalls.h"
#include "vk_types.h"
#include "micro_booter.h"
#include <sys/socket.h>

int rump___sysimpl_socket30(int, int, int);
int rump___sysimpl_bind(int, const struct sockaddr *, socklen_t);

/* These syncronous invocations involve calls to and from a RumpKernel */
extern struct cringbuf *vmrb;

void
rump_io_fn(void *d)
{
	arcvcap_t rcv = SUB_CAPTBL_SELF_IORCV_BASE;

	while (1) {
		int amnt = 0, len = 0;

		cos_rcv(rcv, 0, 0);
		assert(vmrb);

		amnt = cringbuf_sz(vmrb);
		assert(amnt);

		/* TODO: SYNC! */
		printc("+");
		printc("%s", cringbuf_active_extent(vmrb, &len, amnt));
		cringbuf_delete(vmrb, amnt);
	}
}

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
//        paws_tests();

        printc("*** KERNEL COMPONENT RETURNING ***\n\n");

        return ret;

}

int
test_shdmem(int shm_id, int arg2, int arg3, int arg4)
{
	int my_id;
	vaddr_t my_page;

	/* Calling from user component into kernel component */
	assert(!cos_spdid_get());

	/* Map in shared page */
	my_id = shmem_map_invoke(shm_id);
	assert(my_id == shm_id);

	/* Get our vaddr for this shm_id */
	my_page = shmem_get_vaddr_invoke(my_id);
	assert(my_page);

	printc("Kernel Component shared mem vaddr: %p\n", (void *)my_page);
	printc("Reading from page: %c\n", *(char *)my_page);
	printc("Writing 'b' to page + 1\n");
	*((char *)my_page + 1) = 'b';

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
	const struct sockaddr *addr;
	/* TODO use shdmem id to map shdmem here and pass in shdmem pointer as addr */
	shdmem_id = shmem_map_invoke(shdmem_id);
	assert(shdmem_id > -1);
	addr = (const struct sockaddr *)shmem_get_vaddr_invoke(shdmem_id);
	assert(addr > 0);
	return rump___sysimpl_bind(sockfd, addr, addrlen);
}

/* TODO: too many unused arguments in many cases.. get rid of them */
int
rk_inv_entry(int arg1, int arg2, int arg3, int arg4)
{
	int ret = 0;

	/* TODO Rename this dumb conevention from a function to a system call  */
	switch(arg1) {
	case RK_INV_OP1:
		ret = test_fs(arg2, arg3, arg4, 0);
		break;
	case RK_INV_OP2:
		ret = test_shdmem(arg2, arg3, arg4, 0);
		break;
	case RK_GET_BOOT_DONE:
		ret = get_boot_done();
		break;
	case RK_SOCKET:
		ret = rk_socket(arg2, arg3, arg4);
		break;
	case RK_BIND:
		ret = rk_bind(arg2, arg3, (socklen_t)arg4);
		break;
	default: assert(0);
	}

	return ret;
}

#include "vk_types.h"
#include "rumpcalls.h"
#include <rk_inv_api.h>
#include <cos_types.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <posix.h>

extern int vmid;

int
rk_inv_op1(void)
{
	return cos_sinv(APP_CAPTBL_SELF_RK_SINV_BASE, RK_INV_OP1, 0, 0, 0);
}

void
rk_inv_op2(int shmid)
{
	cos_sinv(APP_CAPTBL_SELF_RK_SINV_BASE, RK_INV_OP2, shmid, 0, 0);
}

int
rk_inv_get_boot_done(void)
{
	return cos_sinv(APP_CAPTBL_SELF_RK_SINV_BASE, RK_GET_BOOT_DONE, 0, 0, 0);
}

int
rk_inv_socket(int domain, int type, int protocol)
{
	return cos_sinv(APP_CAPTBL_SELF_RK_SINV_BASE, RK_SOCKET, domain, type, protocol);
}

int
rk_inv_bind(int sockfd, int shdmem_id, socklen_t addrlen)
{
	return cos_sinv(APP_CAPTBL_SELF_RK_SINV_BASE, RK_BIND, sockfd, shdmem_id, addrlen);
}

ssize_t
rk_inv_recvfrom(int s, int buff_shdmem_id, size_t len, int flags, int from_shdmem_id, int fromlenaddr_shdmem_id)
{
	assert(s <= 0xFFFF);
	assert(buff_shdmem_id <= 0xFFFF);
	assert(len <= 0xFFFF);
	assert(flags <= 0xFFFF);
	assert(from_shdmem_id <= 0xFFFF);
	assert(fromlenaddr_shdmem_id <= 0xFFFF);

	return (ssize_t)cos_sinv(APP_CAPTBL_SELF_RK_SINV_BASE, RK_RECVFROM, (s << 16) | buff_shdmem_id,
				(len << 16) | flags, (from_shdmem_id << 16) | fromlenaddr_shdmem_id);
}
/* still using ringbuffer shared data */
int
rk_inv_logdata(void)
{
	return cos_sinv(APP_CAPTBL_SELF_RK_SINV_BASE, RK_LOGDATA, 0, 0, 0);
}

int
rk_socketcall(int call, unsigned long *args)
{
        int ret = -1;

        printc("\tcos_socketcall, call: %d, args: %p\n", call, args);

        switch (call) {
		case 1: { /* Socket */
                        int domain, type, protocol;

                        domain     = *args;
                        type       = *(args + 1);
                        protocol   = *(args + 2);
                        ret = rk_inv_socket(domain, type, protocol);

                        break;
                }
                case 2: { /* Bind */
                        int sockfd, shdmem_id;
                        unsigned long shdmem_addr;
                        void *addr;
                        u32_t addrlen;

                        sockfd  = *args;
                        addr    = *(args + 1);
                        addrlen = *(args + 2);

                        /*
                         * Do stupid shared memory for now
                         * allocate a page for each bind addr
                         * don't deallocate. #memLeaksEverywhere
                         */

                        shdmem_id = shmem_allocate_invoke();
                        assert(shdmem_id > -1);

                        shdmem_addr = shmem_get_vaddr_invoke(shdmem_id);
                        assert(shdmem_addr > 0);

                        memcpy(shdmem_addr, addr, addrlen);
                        ret = rk_inv_bind(sockfd, shdmem_id, addrlen);

                        break;
                }
		case 12: { /* Recvfrom */
                        int s, flags, shdmem_id;
                        unsigned long shdmem_addr;
                        void *buff;
                        size_t len;
                        struct sockaddr *from_addr;
                        u32_t *from_addr_len_ptr;

                        s                 = *args;
                        buff              = *(args + 1);
                        len               = *(args + 2);
                        flags             = *(args + 3);
                        from_addr         = *(args + 4);
                        from_addr_len_ptr = *(args + 5);

                        /* For the time being, just allocate a new page every time we read */
                        /* TODO don't do that */
                        shdmem_id = shmem_allocate_invoke();
                        assert(shdmem_id > -1);

                        shdmem_addr = shmem_get_vaddr_invoke(shdmem_id);
                        assert(shdmem_addr > 0);

                        ret = (int)rk_inv_recvfrom(s, shdmem_id, len, flags,
                                shdmem_id, *from_addr_len_ptr);

                        /* TODO, put this in a function */
                        /* Copy buffer back to its original value*/
                        memcpy(buff, shdmem_addr, ret);
                        shdmem_addr += len; /* Add overall length of buffer */

                        /* Set from_addr_len_ptr pointer to be shared memory at right offset */
                        *from_addr_len_ptr = *(u32_t *)shdmem_addr;
                        shdmem_addr += sizeof(u32_t *);

                        /* Copy from_addr to be shared memory at right offset */
                        memcpy(from_addr, shdmem_addr, *from_addr_len_ptr);

                        break;
                }
                default:
                        assert(0);
        }

        return ret;
}

int
rk_socketcall_init(void)
{
	assert(vmid != 0);

	posix_syscall_override((cos_syscall_t)rk_socketcall, __NR_socketcall);

	return 0;
}

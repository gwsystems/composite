#include "vk_types.h"
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

/* still using ringbuffer shared data */
int
rk_inv_logdata(void)
{
	return cos_sinv(APP_CAPTBL_SELF_RK_SINV_BASE, RK_LOGDATA, 0, 0, 0);
}

unsigned int cos_spdid_get(void);
int shmem_allocate_invoke(void);
unsigned long shmem_get_vaddr_invoke(int id);
void *memcpy(void *dest, const void *src, size_t n);
int
rk_socketcall(int call, unsigned long *args)
{
        int ret = -1;

        printc("\tcos_socketcall, call: %d, args: %p\n", call, args);

        switch (call) {
                case 1: {
                        int domain, type, protocol;

                        domain     = *args;
                        type       = *(args+1);
                        protocol   = *(args+2);
                        ret = rk_inv_socket(domain, type, protocol);

                        break;
                }
                case 2: {
                        int sockfd, shdmem_id;
                        unsigned long shdmem_addr;
                        void *addr;
                        u32_t addrlen;

                        sockfd  = *args;
                        addr    = (void *)*(args+1);
                        addrlen = *(args+2);

                        /*
                         * Do stupid shared memory for now
                         * allocate a page for each bind addr
                         * don't deallocate. #memLeaksEverywhere
                         */

                        shdmem_id = shmem_allocate_invoke();
                        assert(shdmem_id > -1);

                        shdmem_addr = shmem_get_vaddr_invoke(shdmem_id);
                        assert(shdmem_addr > 0);

                        memcpy((void *)shdmem_addr, addr, addrlen);
                        ret = rk_inv_bind(sockfd, shdmem_id, addrlen);

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

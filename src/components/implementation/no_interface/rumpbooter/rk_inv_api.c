#include "vk_types.h"
#include <rk_inv_api.h>
#include <cos_types.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>

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

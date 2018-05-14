#ifndef RK_CALLS_H
#define RK_CALLS_H

#include <rk.h>
#include <rk_types.h>

static inline vaddr_t
rk_api(rk_api_t api)
{
	vaddr_t apiaddr = 0;

	switch(api) {
	case TEST_ENTRY:
		apiaddr = (vaddr_t)&test_entry;
		break;
	case TEST_FS:
		apiaddr = (vaddr_t)&test_fs;
		break;
	case GET_BOOT_DONE:
		apiaddr = (vaddr_t)&get_boot_done;
		break;
	case RK_SOCKET:
		apiaddr = (vaddr_t)&rk_socket;
		break;
	case RK_BIND:
		apiaddr = (vaddr_t)&rk_bind;
		break;
	case RK_RECVFROM:
		apiaddr = (vaddr_t)&rk_recvfrom;
		break;
	case RK_SENDTO:
		apiaddr = (vaddr_t)&rk_sendto;
		break;
	case RK_SETSOCKOPT:
		apiaddr = (vaddr_t)&rk_setsockopt;
		break;
	case RK_MMAP:
		apiaddr = (vaddr_t)&rk_mmap;
		break;
	case RK_WRITE:
		apiaddr = (vaddr_t)&rk_write;
		break;
	case RK_READ:
		apiaddr = (vaddr_t)&rk_read;
		break;
	case RK_LISTEN:
		apiaddr = (vaddr_t)&rk_listen;
		break;
	case RK_CLOCK_GETTIME:
		apiaddr = (vaddr_t)&rk_clock_gettime;
		break;
	case RK_SELECT:
		apiaddr = (vaddr_t)&rk_select;
		break;
	case RK_ACCEPT:
		apiaddr = (vaddr_t)&rk_accept;
		break;
	case RK_OPEN:
		apiaddr = (vaddr_t)&rk_open;
		break;
	case RK_UNLINK:
		apiaddr = (vaddr_t)&rk_unlink;
		break;
	case RK_FTRUNCATE:
		apiaddr = (vaddr_t)&rk_ftruncate;
		break;
	case RK_GETSOCKNAME:
		apiaddr = (vaddr_t)&rk_getsockname;
		break;
	case RK_GETPEERNAME:
		apiaddr = (vaddr_t)&rk_getpeername;
		break;
	default: assert(0);
	}

	return apiaddr;
}



#endif /* RK_CALLS_H */

#ifndef INTERFACE_IMPL_H
#define INTERFACE_IMPL_H

#include <cos_types.h>
#include <cos_component.h>
#include <shm_bm.h>

#define NIC_MAX_SESSION 256
#define NIC_MAX_SHEMEM_REGION 3

struct shemem_info {
	cbuf_t shmid;
	shm_bm_t shm;

	paddr_t paddr;
};
/* per thread session */
struct client_session {
	struct shemem_info shemem_info[NIC_MAX_SHEMEM_REGION];

	u32_t ip_addr; 
	u16_t port;
};

#endif /* INTERFACE_IMPL_H */
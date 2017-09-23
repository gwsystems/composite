#ifndef VK_STRUCTS_H
#define VK_STRUCTS_H

#include <cos_types.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <sl.h>
#include <sl_thd.h>

struct vms_info {
	unsigned int           id;
	struct cos_defcompinfo dci;
	struct cos_compinfo    shm_cinfo;
	struct sl_thd         *inithd;
	sinvcap_t              sinv;

	/* Created based on the policy */
	thdcap_t  iothd;
	arcvcap_t iorcv;
	tcap_t    iotcap;
	asndcap_t ioasnd;
};

struct vkernel_info {
	struct cos_compinfo shm_cinfo;

	thdcap_t  termthd;
	sinvcap_t sinv;
};

extern struct vms_info     vmx_info[];
extern struct vkernel_info vk_info;

#endif /* VK_STRUCTS_H */

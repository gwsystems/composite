#ifndef VK_STRUCTS_H
#define VK_STRUCTS_H

#include <cos_types.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <sl.h>
#include <sl_thd.h>

struct vm_io_info {
	thdcap_t  iothd;
	arcvcap_t iorcv;
	asndcap_t ioasnd;
};

struct dom0_io_info {
	thdcap_t  iothds[VM_COUNT - 1];
	arcvcap_t iorcvs[VM_COUNT - 1];
	asndcap_t ioasnds[VM_COUNT - 1];
};

struct vms_info {
	unsigned int           id;
	struct cos_defcompinfo dci;
	struct cos_compinfo    shm_cinfo;
	struct sl_thd         *inithd;
	sinvcap_t              sinv;

	union { /* for clarity */
		struct vm_io_info *  vmio;
		struct dom0_io_info *dom0io;
	};
};

struct vkernel_info {
	struct cos_compinfo shm_cinfo;

	thdcap_t  termthd;
	sinvcap_t sinv;
	asndcap_t vminitasnd[VM_COUNT];
};

extern struct vms_info     vmx_info[];
extern struct dom0_io_info dom0ioinfo;
extern struct vm_io_info   vmioinfo[];
extern struct vkernel_info vk_info;

#endif /* VK_STRUCTS_H */

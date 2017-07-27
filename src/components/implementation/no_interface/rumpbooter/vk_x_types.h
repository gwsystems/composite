#ifndef VK_X_TYPES_H
#define VK_X_TYPES_H

#include "vk_types.h"
#include <cos_kernel_api.h>

struct vm_io_info {
        thdcap_t iothd;
        arcvcap_t iorcv;
        asndcap_t ioasnd;
};

struct dom0_io_info {
        thdcap_t iothds[VM_COUNT-1];
        tcap_t iotcaps[VM_COUNT-1];
        arcvcap_t iorcvs[VM_COUNT-1];
        asndcap_t ioasnds[VM_COUNT-1];
};

struct vms_info {
        unsigned int id;
        struct cos_compinfo cinfo, shm_cinfo;

        unsigned int state;
        thdcap_t initthd, exitthd;
        thdid_t inittid;
        tcap_t inittcap;
        arcvcap_t initrcv;

        union { /* for clarity */
                struct vm_io_info *vmio;
                struct dom0_io_info *dom0io;
        };
};

struct vkernel_info {
        struct cos_compinfo cinfo, shm_cinfo;

        thdcap_t termthd;
        asndcap_t vminitasnd[VM_COUNT];
};

#endif /* VK_X_TYPES_H */

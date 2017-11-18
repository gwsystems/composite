#include "vk_types.h"
#include "vk_structs.h"
#include "vk_api.h"
#include <shdmem.h>

/* These syncronous invocations are related to calling to and from the vkernel */

extern int vmid;
extern int shmem_call(int arg1, int arg2, int arg3, int arg4);

int
vk_vm_id(void)
{
	assert(0);
	return 0;
}

void
vk_vm_exit(void)
{
	assert(0);
}

void
vk_vm_block(tcap_time_t timeout)
{
	assert(0);
}

/* TODO: can pack some args */
vaddr_t
vk_shmem_vaddr_get(int spdid, int id)
{
	assert(0);
	return 0;
}

int
vk_shmem_alloc(int spdid, int i)
{
	assert(0);
	return 0;
}

int
vk_shmem_dealloc(void)
{
	assert(0);
	return 0;
}

int
vk_shmem_map(int spdid, int id)
{
	assert(0);
	return 0;
}

static inline int
vkernel_find_vm(thdid_t tid)
{
	int i;


	for (i = 0 ; i < VM_COUNT ; i ++) {
		if (vmx_info[i].inithd && (vmx_info[i].inithd)->thdid == tid) break;
	}
	assert (i < VM_COUNT);

	return i;
}

int
shmem_call(int arg1, int arg2, int arg3, int arg4)
{
        int ret = 0;

        switch(arg1) {
        case VK_SERV_SHM_VADDR_GET:
                ret = (int)shm_get_vaddr((unsigned int)arg2, (unsigned int)arg3);
                break;
        case VK_SERV_SHM_ALLOC:
                ret = shm_allocate((unsigned int)arg2, arg3);
                break;
        case VK_SERV_SHM_DEALLOC:
                ret = shm_deallocate(arg2, arg3, arg4, 0);
                break;
        case VK_SERV_SHM_MAP:
                ret = shm_map(arg2, arg3);
                break;
        default: assert(0);
        }
        return ret;
}

int
vkernel_hypercall(int a, int b, int c, int d)
{
	int option = a >> 16;
	int thdid  = (a << 16) >> 16;
	int ret = 0;
	int i;

	switch(option) {
	case VK_SERV_VM_EXIT:
	{
		/* free sl threads */
		i = vkernel_find_vm(thdid);

		printc("%s %d EXIT\n", i < APP_START_ID ? "VM" : "APP", i);

		if (i < APP_START_ID) sl_thd_free(vmx_info[i].inithd);
		else                  return 0;

		/* TODO: Free all the resources allocated for this VM! -Initial capabilites, I/O Capabilities etc */
		printc("VM %d ERROR!!!!!", i);
		break;
	}
	default: ret = shmem_call(option, b, c, d);
	}

	return ret;
}

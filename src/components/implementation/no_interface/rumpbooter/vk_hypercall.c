#include "vk_types.h"
#include "vk_structs.h"
#include "vk_api.h"

extern int vmid;

int
vk_vm_id(void)
{
	return cos_sinv(VM_CAPTBL_SELF_SINV_BASE, VK_SERV_VM_ID << 16 | cos_thdid(), 0, 0, 0);	
}

void
vk_vm_exit(void)
{
	cos_sinv(VM_CAPTBL_SELF_SINV_BASE, VK_SERV_VM_EXIT << 16 | vmid, 0, 0, 0);
}

void
vk_vm_block(tcap_time_t timeout)
{
	cos_sinv(VM_CAPTBL_SELF_SINV_BASE, VK_SERV_VM_ID << 16 | vmid, (int)timeout, 0, 0);
}

static inline int
vkernel_find_vm(thdid_t tid)
{
	int i;

	for (i = 0 ; i < VM_COUNT ; i ++) {
		if ((vmx_info[i].inithd)->thdid == tid) break;
	}
	assert (i < VM_COUNT);

	return i;
}

int
vkernel_hypercall(int a, int b, int c)
{
	int option = a >> 16;
	int thdid  = (a << 16) >> 16;
	int ret = 0;
	int i = thdid;

	switch(option) {
	case VK_SERV_VM_EXIT:
	{
		/* free sl threads */
		i = vkernel_find_vm(thdid);
		printc("VM%d EXIT\n", i);
		sl_thd_free(vmx_info[i].inithd);

		/* TODO: Free all the resources allocated for this VM! -Initial capabilites, I/O Capabilities etc */

		printc("VM %d ERROR!!!!!", i);
		break;
	}
	case VK_SERV_VM_ID:
	{
		ret = vmx_info[i].id;

		break;
	}
	case VK_SERV_VM_BLOCK:
	{
		tcap_time_t timeout     = (tcap_time_t)b;
		cycles_t abs_timeout, now;

		rdtscll(now);
		abs_timeout = tcap_time2cyc(timeout, now);

		/* calling thread must be the main thread! */
		sl_thd_block_timeout(0, abs_timeout);
		break;
	}
	default: assert(0);
	}

	return ret;
}

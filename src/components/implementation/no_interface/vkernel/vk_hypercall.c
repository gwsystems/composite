
#include "vk_types.h"

static inline int
vkernel_find_vm(thdid_t tid)
{
	int i;

	for (i = 0 ; i < VM_COUNT ; i ++) {
		if (sl_thd_thdid(vmx_info[i].inithd) == tid) break;
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
	int i;

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
		i = vkernel_find_vm(thdid);
		ret = vmx_info[i].id;

		break;
	}
	default: assert(0);
	}

	return ret;
}

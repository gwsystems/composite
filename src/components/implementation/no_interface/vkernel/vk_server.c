
#include "vk_types.h"

int
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
vkernel_server(int a, int b, int c)
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

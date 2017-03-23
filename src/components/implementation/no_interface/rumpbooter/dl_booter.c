#include <stdio.h>
#include <string.h>
#include <cos_component.h>

#include <cobj_format.h>
#include <cos_kernel_api.h>

#include "rumpcalls.h"
#include "cos_init.h"

extern struct cos_compinfo booter_info;
extern int vmid;

void 
dl_booter_init(void)
{
	printc("DL_BOOTER_INIT: %d\n", vmid);
	while(1) {
		cos_rcv(VM0_CAPTBL_SELF_IORCV_SET_BASE);
		printc("intr recieved \n");
		int i;
		for(i = 0; i < 10; i ++) {
			
		}
	}
}	

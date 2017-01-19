#include "assert.h"
#include "kernel.h"
#include "boot_comp.h"
#include "chal_cpu.h"
#include "mem_layout.h"
#include "string.h"
#include <pgtbl.h>
#include <thd.h>
#include <component.h>
#include <inv.h>
#include <hw.h>

/* The test component that is directly called */
extern void comp1(void);

void
kern_boot_comp(void)
{

}


void
kern_boot_upcall(void)
{
	/* u8_t *entry = mem_bootc_entry(); */
	u32_t flags = 0;
	void *p;

	/* printk("Upcall into boot component at ip 0x%x\n", entry); */
	printk("------------------[ Kernel boot complete ]------------------\n");

	chal_user_upcall(comp1, thd_current(cos_cpu_local_info())->tid);
	assert(0);		/* should never get here! */
}


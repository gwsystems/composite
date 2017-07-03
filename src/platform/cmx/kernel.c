#include "assert.h"
#include "kernel.h"
#include "string.h"

#include <captbl.h>
#include <retype_tbl.h>
#include <component.h>
#include <thd.h>
#include "stm32f7xx_hal.h"
#include "core_cm7.h"
#include "boot_comp.h"

#include "cos_component.h"
#include "mem_layout.h"

#include "lcd.h"

struct mem_layout glb_memlayout;

extern void cos_init(void);
extern void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3);
extern void hw_timer_set(cycles_t cycles);
extern void hw_timer_disable(void);
extern void hw_init(void);

extern unsigned int __utmem_end__;
void
kern_memory_setup(void)
{
	/* Some memory initialization with fixed parameters */
	glb_memlayout.kern_end=(u8_t*)COS_CMX_RAM_START;
	/* Currently we dont have any modules, mod_end unused */
	glb_memlayout.mod_start=(u8_t*)COS_CMX_FIRST_PAGE;
	glb_memlayout.mod_end=(u8_t*)COS_CMX_SECOND_PAGE-1;
	glb_memlayout.bootc_entry=glb_memlayout.bootc_vaddr=(u8_t*)cos_upcall_fn;
	glb_memlayout.kern_boot_heap=(u8_t*)COS_CMX_SECOND_PAGE;
	glb_memlayout.kmem_end=(u8_t*)(((u32_t)&__utmem_end__)-1); /* Should be 0x2007FFFF on CM7 */
	glb_memlayout.allocs_avail=1;

	/* Validate the memory layout. */
	assert(mem_boot_start()  >= mem_kmem_start());
	assert(mem_kmem_start()  == mem_bootc_start());
	assert(mem_kmem_end()    >= mem_boot_end());
	assert(mem_utmem_start() >= mem_kmem_start());
	assert(mem_utmem_start() >= mem_boot_end());
	assert(mem_utmem_end()   <= mem_kmem_end());
	assert(mem_bootc_entry() - mem_bootc_vaddr() <= mem_bootc_end() - mem_bootc_start());
}

extern void hw_init(void);

void
main(void)
{
	hw_init();

	kern_memory_setup();

	chal_init();
	cap_init();
	ltbl_init();
	retype_tbl_init();
	comp_init();
	thd_init();

	kern_boot_comp();
	kern_boot_upcall();
	/* should not get here... */
	khalt();
}

void
khalt(void)
{
	printk("Shutting down...\n");
	while(1);
}

void
printk(const char *fmt, ...)
{

}

void assert_failed(uint8_t* file, uint32_t line)
{

}

void
chal_tls_update(vaddr_t vaddr)
{

}

void
chal_timer_set(cycles_t cycles)
{
	hw_timer_set(cycles);
}

void
chal_timer_disable(void)
{
	hw_timer_disable();
}

int
chal_cyc_usec(void)
{ return 216; }

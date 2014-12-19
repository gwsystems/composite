#define ENABLE_SERIAL
#define ENABLE_TIMER

#include "assert.h"
#include "kernel.h"
#include "multiboot.h"
#include "string.h"
#include "comp.h"

#include <captbl.h>
#include <retype_tbl.h>
#include <component.h>
#include <thd.h>
#include <inv.h>

void kmain(struct multiboot *mboot, u32_t mboot_magic, u32_t esp);

void
khalt(void)
{
	printk("Shutting down...\n");
	asm("mov $0x53,%ah");
	asm("mov $0x07,%al");
	asm("mov $0x001,%bx");
	asm("mov $0x03,%cx");
	asm("int $0x15");
}

void 
kmain(struct multiboot *mboot, u32_t mboot_magic, u32_t esp)
{
	int bc = 0;
        struct spd_info spd_info;

	tss_init();
	gdt_init();
	idt_init();

#ifdef ENABLE_SERIAL
	serial_init();
#endif

#ifdef ENABLE_CONSOLE
	console_init();
#endif

#ifdef ENABLE_TIMER
	timer_init(100);
#endif
  
	if (mboot_magic != MULTIBOOT_EAX_MAGIC) {
		die("Not started from a multiboot loader!\n");
	}

	cap_init();
       	ltbl_init();
       	retype_tbl_init();
       	comp_init();
       	thd_init();
       	inv_init();

	paging_init(mboot->mods_count, (u32_t*)mboot->mods_addr);

        spd_info.mem_size = (unsigned long)mboot->size;
	assert(kern_boot_comp(&spd_info) == 0);

	user_init();
	khalt(); 
}

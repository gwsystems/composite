#define ENABLE_SERIAL
#define ENABLE_TIMER

#include "kernel.h"
#include "multiboot.h"
#include "string.h"

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

	paging_init(mboot->mem_lower + mboot->mem_upper, mboot->mods_count, (u32_t*)mboot->mods_addr);
	user_init();
	khalt(); 
}

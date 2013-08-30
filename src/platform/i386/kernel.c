#include "multiboot.h"
#include "printk.h"
#include "serial.h"
#include "types.h"
#include "string.h"
#include "boot_log.h"
#include "timer.h"
#include "gdt.h"
#include "ports.h"
#include "idt.h"
#include "isr.h"
#include "vm.h"
#include "kbd.h"
#include "user.h"

void kmain(struct multiboot *mboot, uintptr_t mboot_magic, uintptr_t esp);
int keep_kernel_running = 1;


void 
kmain(struct multiboot *mboot, uintptr_t mboot_magic, uintptr_t esp)
{
    printk__init();
    printk(INFO, "Booting....\n"); 
   
    printk(INFO, "Enabling serial\n");
    serial__init();
    
    printk(INFO, "Turning on serial prink\n");
    printk__register_handler(&serial__puts);

    printk(INFO, "Enabling gdt\n");
    gdt__init();

    printk(INFO, "Enabling idt\n");
    idt__init();

    printk(INFO, "Enabling timer\n");
    timer__init(100);

    printk(INFO, "Enabling keyboard\n");
    kbd__init();

    boot_log("Initalizing Multiboot");
    boot_log_finish(BOOT_OK);

    if (mboot_magic == MULTIBOOT_EAX_MAGIC) {
        printk(INFO, "Multiboot kernel\n");
	multiboot__print(mboot);

/*
        if ((char *)mboot->cmdline != NULL)
            printk(INFO, "cmdline: %s\n", (char *)mboot->cmdline);
	printk(INFO, "\tFlags: 0x%x\n", mboot->flags);
	printk(INFO, "\tMemory: 0x%x:0x%x\n", mboot->mem_lower, mboot->mem_upper);
	if (mboot->boot_device != 0) printk(INFO, "\tBoot Device: %s\n", mboot->boot_device);
	printk(INFO, "\tModules: %d (%x)\n", mboot->mods_count, mboot->mods_addr);
*/
        
        
        printk(INFO, "Mem Size: %d\n", mboot->mem_lower + mboot->mem_upper);
        
        paging__init(mboot->mem_lower + mboot->mem_upper);
    }

    printk(INFO, "Enabling interrupts\n");
    asm volatile ("sti");
    
#undef FORCE_PAGE_FAULT
#ifdef FORCE_PAGE_FAULT
    printk(INFO, "Forcing page fault\n");
    uintptr_t *ptr = (uintptr_t *)0xA0000000;
    printk(INFO, "pfault %d\n", *ptr);
#endif

    user__init ();

    while (keep_kernel_running);

    printk(INFO, "Shutting down...\n");
    asm("mov $0x53,%ah");
    asm("mov $0x07,%al");
    asm("mov $0x001,%bx");
    asm("mov $0x03,%cx");
    asm("int $0x15");
}

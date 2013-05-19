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

void kmain(struct multiboot *mboot, uintptr_t mboot_magic, uintptr_t esp);
int keep_kernel_running = 1;


void 
kmain(struct multiboot *mboot, uintptr_t mboot_magic, uintptr_t esp)
{
    int i;
    
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

        if ((char *)mboot->cmdline != NULL)
            printk(INFO, "cmdline: %s\n", (char *)mboot->cmdline);
        
        
        printk(INFO, "Mem Size: %d\n", mboot->mem_lower + mboot->mem_upper);
        
        paging__init(mboot->mem_lower + mboot->mem_upper);
    }

    printk(INFO, "Hello World\n");
    printk(CRITICAL, "Hello World\n");
    printk(WARN, "This is a multiline kernel\n");
    for (i = 0; i < 8; i++)
        printk(ERROR, "Hello bob %d\n", i);

    printk(INFO, "Enabling interrupts\n");
    asm volatile ("sti");
    

#ifdef FORCE_PAGE_FAULT
    printk(INFO, "Forcing page fault\n");
    uintptr_t *ptr = (uintptr_t *)0xA0000000;
    printk(INFO, "pfault %d\n", *ptr);
#endif

    while (keep_kernel_running);

    printk(INFO, "Shutting down...");
}


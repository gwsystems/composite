#include "multiboot.h"
#include "printk.h"
#include "serial.h"
#include "shared/cos_types.h"
#include "string.h"
#include "timer.h"
#include "gdt.h"
#include "ports.h"
#include "idt.h"
#include "isr.h"
#include "vm.h"
#include "kbd.h"
#include "tss.h"
#include "user.h"

void kmain(struct multiboot *mboot, u32_t mboot_magic, u32_t esp);
extern u32_t user_size;
extern u32_t base_user_address;
int keep_kernel_running = 1;

void 
kmain(struct multiboot *mboot, u32_t mboot_magic, u32_t esp)
{
  printk__init();
  printk(INFO, "Booting....\n"); 
  
  printk(INFO, "Enabling serial\n");
  serial__init();
  
  printk(INFO, "Turning on serial printk\n");
  printk__register_handler(&serial__puts);

  printk(INFO, "Setting up the TSS\n");
  tss__init ();
  
  printk(INFO, "Enabling gdt\n");
  gdt__init();
  
  printk(INFO, "Enabling idt\n");
  idt__init();
  
  printk(INFO, "Enabling timer\n");
  timer__init(100);
  
  printk(INFO, "Enabling keyboard\n");
  kbd__init();
  
  printk(INFO, "Initalizing Multiboot\n");
  
  if (mboot_magic == MULTIBOOT_EAX_MAGIC) {
    printk(INFO, "Multiboot kernel\n");
    printk(INFO, "Mem Size: %d\n", mboot->mem_lower + mboot->mem_upper);
    base_user_address = (u32_t)(mboot->mods_addr + 0x1000);
    user_size = multiboot__print(mboot) - base_user_address;
  } else {
    die("Not started from a multiboot loader!\n");
  }

  printk(INFO, "Enabling virtual memory\n");
  paging__init(mboot->mem_lower + mboot->mem_upper, mboot->mods_count, (u32_t*)mboot->mods_addr);
  
  printk(INFO, "Enabling interrupts\n");
  asm volatile ("sti");
  
  printk(INFO, "Jumping to user mode\n");
  user__init ();

  //while (keep_kernel_running);
  
  printk(INFO, "Shutting down...\n");
  asm("mov $0x53,%ah");
  asm("mov $0x07,%al");
  asm("mov $0x001,%bx");
  asm("mov $0x03,%cx");
  asm("int $0x15");
}

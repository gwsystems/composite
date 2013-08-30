#include "types.h"
#include "printk.h"
#include "string.h"
#include "isr.h"
#include "mm.h"
#include "vm.h"

//#define KERNEL_TABLES 64	// 256 MB
#define KERNEL_TABLES 4 

extern void user_test (void);
extern void user_test_end (void);

uint32_t kerndir[1024] __attribute__((aligned(4096)));
uint32_t userdir[1024] __attribute__((aligned(4096)));

uint32_t kernel_pagetab[KERNEL_TABLES][1024] __attribute__((aligned(4096)));
//uint32_t sys_pagetab[1024] __attribute__((aligned(4096)));
uint32_t user_pagetab[1024] __attribute__((aligned(4096)));

static void
load_page_directory(uint32_t *dir)
{
    uint32_t d = (uint32_t)chal_va2pa(dir) | PAGE_P;
     
    printk (INFO, "Setting cr3 = %x (%x)\n", d, d);
    asm volatile("mov %0, %%cr3" : : "r"(d));
}

void
switch_user_mode(void)
{
  static int usermode = 0;
  uint32_t *sent;
  sent = (uint32_t*)0x1400000;
  if (usermode) {
    printk(INFO, "Switching to kernel mode\n");
    load_page_directory(kerndir);
    usermode = 0;
  } else {
    printk(INFO, "Switching to user mode\n"); 
    load_page_directory(userdir);
    usermode = 1;
  }
  printk(INFO, "Contents of address %x: %x\n", sent, *sent);
}

void *
chal_pa2va(void *address)
{
    return address;
}

void *
chal_va2pa(void *address)
{
    return address;
}


static void
page_fault(struct registers *regs)
{
    uintptr_t fault_addr;
    
    asm volatile("mov %%cr2, %0" : "=r" (fault_addr));

    die("Page Fault (%s%s%s%s) at 0x%X\n",
        !(regs->err_code & PAGE_P) ? "present " : "",
        regs->err_code & PAGE_RW ? "read-only " : "",
        regs->err_code & PAGE_US ? "user-mode " : "",
        regs->err_code & PAGE_PCD ? "reserved " : "",
        fault_addr);

}

static void
map_table(uint32_t *dir, uint32_t tno, uint32_t *table, uint32_t flags)
{
  dir[tno] = (((uint32_t) table) & PAGE_FRAME) | flags;
  //printk(INFO, "Setting dir[%d] to %x (from %x)\n", tno, dir[tno], table);
}

static void
init_table(uint32_t *table, uint32_t *base, uint32_t flags)
{
  int i;
  //printk(INFO, "Initializing table at %x from base %x\n", table, base);
  for (i = 0; i < 1024; i++) {
    table[i] = (((uint32_t) base + (4096 * i)) & PAGE_FRAME) | flags;
    //if (i < 3) printk (INFO, "\t%x\n", table[i]);
  } 
}

void
paging__init(size_t memory_size)
{
    uint32_t cr0;
    int i;
    printk(INFO, "Intialize paging\n");
    printk(INFO, "MEMORY_SIZE: %dMB\n", memory_size/1024);

    printk(INFO, "Registering handler\n");
    register_interrupt_handler(14, &page_fault);

    printk(INFO, "Mapping pages to tables and directories\n");


    for (i = 0; i <= KERNEL_TABLES; i++) {
      //uint32_t *t;

      init_table(kernel_pagetab[i], (uint32_t*) (i * 4096 * 1024), PAGE_RW | PAGE_P | PAGE_G);
      map_table(kerndir, i, kernel_pagetab[i], PAGE_RW | PAGE_P | PAGE_G);
      map_table(userdir, i, kernel_pagetab[i], PAGE_RW | PAGE_P | PAGE_G);

      //t = (uint32_t *)(pagedir[i] & PAGE_FRAME);
      //printk(INFO, "pd[%d] => %x:  %x, %x, %x...\n", i, pagedir[i], t[0], t[1], t[2]);
    }

    //init_table(sys_pagetab, (uint32_t*) (KERNEL_TABLES * 4096 * 1024), PAGE_RW | PAGE_P);
    map_table(kerndir, KERNEL_TABLES + 1, kernel_pagetab[0], PAGE_RW | PAGE_P);

    init_table(user_pagetab, (uint32_t*) (KERNEL_TABLES * 4096 * 1024), PAGE_RW | PAGE_P | PAGE_US);
    map_table(userdir, KERNEL_TABLES + 1, user_pagetab, PAGE_RW | PAGE_P | PAGE_US);

    printk(INFO, "Base user page is at %x\n", user_pagetab[0]);

    printk(INFO, "Loading page directory\n");
    load_page_directory(kerndir);

    printk(INFO, "Enabling paging\n");
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    printk (INFO, "cr0 == %x\n", cr0);
    cr0 |= 0x80000000;
    printk (INFO, "cr0 = %x\n", cr0 | 0x80000000);
    asm volatile("mov %0, %%cr0" : : "r"(cr0));
    printk (INFO, "OK\n");

    // setting some stuff for testing
    memcpy((uint32_t*)(user_pagetab[0] & PAGE_FRAME), &user_test, user_test_end - user_test);

    printk(INFO, "Finished\n");
}

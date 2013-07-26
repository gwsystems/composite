#include "types.h"
#include "printk.h"
#include "string.h"
#include "isr.h"
#include "mm.h"
#include "vm.h"

uint32_t pagedir[1024] __attribute__((aligned(4096)));
uint32_t kernel_pagetab[1024] __attribute__((aligned(4096)));

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
load_page_directory(size_t dir)
{
    uint32_t d = (uint32_t)chal_va2pa(pagedir) | PAGE_P;
     
    printk (INFO, "Setting cr3 = %x (%x)\n", d, d);
    asm volatile("mov %0, %%cr3" : : "r"(d));
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
    for (i = 0; i < 1024; i++) {
        pagedir[i] = ((((uint32_t) &kernel_pagetab) + (i * 4096)) & PAGE_FRAME) | PAGE_RW | (i == 0 ? PAGE_P | PAGE_G : PAGE_US);
        kernel_pagetab[i] = ((4096 * (i)) & PAGE_FRAME) | PAGE_RW | PAGE_P | PAGE_G;
    }

    printk(INFO, "Loading page directory\n");
    load_page_directory(0);

    printk(INFO, "Enabling paging\n");
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    printk (INFO, "cr0 == %x\n", cr0);
    cr0 |= 0x80000000;
    printk (INFO, "cr0 = %x\n", cr0 | 0x80000000);
    asm volatile("mov %0, %%cr0" : : "r"(cr0));
    printk (INFO, "OK\n");

    printk(INFO, "Finished\n");
}

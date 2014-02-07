#include "types.h"
#include "printk.h"
#include "string.h"
#include "isr.h"
#include "vm.h"

#define POSSIBLE_FRAMES 1024*1024

uint32_t user_size;
uint32_t *base_user_address;

ptd_t kerndir __attribute__((aligned(4096)));
pt_t kernel_pagetab[1024] __attribute__((aligned(4096)));
pt_t user_pagetab[1024] __attribute__((aligned(4096)));

void
ptd_load(ptd_t dir)
{
    uint32_t d = (uint32_t)chal_va2pa(dir) | PAGE_P;
     
    printk (INFO, "Setting cr3 = %x (%x)\n", d, d);
    asm volatile("mov %0, %%cr3" : : "r"(d));
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
    uintptr_t fault_addr, cs, eip = 0;
    
    asm volatile("mov %%cr2, %0" : "=r" (fault_addr));
    //asm volatile("mov %%eip, %0" : "=r" (eip));
    asm volatile("mov %%cs, %0" : "=r" (cs));

    die("Page Fault (%s%s%s%s) at 0x%x, eip 0x%x, cs 0x%x\n",
        !(regs->err_code & PAGE_P) ? "not-present " : "",
        regs->err_code & PAGE_RW ? "read-only " : "read-fault ",
        regs->err_code & PAGE_US ? "user-mode " : "system ",
        regs->err_code & PAGE_RSVD ? "reserved " : "",
        regs->err_code & PAGE_ID ? "instruction-fetch " : "",
        fault_addr, eip, cs);

}

static void
ptd_map(ptd_t dir, uint32_t tno, pt_t table, uint32_t flags)
{
  dir[tno] = (((uint32_t) table) & PAGE_FRAME) | flags;
}

void
ptd_init(ptd_t dir)
{
  int i;
  for (i = 0; i < 1024; i++) {
    dir[i] = 0;
  }
}

#if 0
static void
pt_map(pt_t table, uint32_t eno, pte_t entry, uint32_t flags)
{
  table[eno] = (((uint32_t) entry) & PAGE_FRAME) | flags;
}
#endif


void
ptd_copy_global(ptd_t dst, ptd_t src)
{
  int i = 0;
  for (i = 0; i < 1024; i++) {
    if (src[i] & PAGE_G) {
      dst[i] = src[i];
    }
  }
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
paging__init(size_t memory_size, uint32_t nmods, uint32_t *mods)
{
    uint32_t cr0;
    u32_t i;

    printk(INFO, "Initializing paging\n");
    printk(INFO, "MEMORY_SIZE: %dMB (%d page frames)\n", memory_size/1024, memory_size/4);

    printk(INFO, "Registering page fault handler\n");
    register_interrupt_handler(14, &page_fault);

    printk(INFO, "Mapping pages to tables and directories\n");

    ptd_init(kerndir);

    for (i = 0; i < (uint32_t)base_user_address / (PAGE_SIZE * 1024); i++) {
      init_table(kernel_pagetab[i], (uint32_t*) (i * 4096 * 1024), PAGE_RW | PAGE_P | PAGE_G);
      ptd_map(kerndir, i, kernel_pagetab[i], PAGE_RW | PAGE_P | PAGE_G);
    }

    for (i = 0; i <= (user_size / PAGE_SIZE)+1; i++) {
      init_table(user_pagetab[i],
        (uint32_t*) ((uint32_t)base_user_address) + (i * 4096 * 1024),
        PAGE_RW | PAGE_P | PAGE_US);
      ptd_map(kerndir, (SERVICE_START >> 22) + i, user_pagetab[i], PAGE_RW | PAGE_P | PAGE_US);
    }

    printk(INFO, "Base physical address 0x%x, size 0x%x, mapping at 0x%x\n", base_user_address, user_size, SERVICE_START);

    printk(INFO, "Loading page directory\n");
    ptd_load(kerndir);

    printk(INFO, "Enabling paging\n");
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    asm volatile("mov %0, %%cr0" : : "r"(cr0));
    printk (INFO, "OK\n");

    printk(INFO, "Finished\n");
}

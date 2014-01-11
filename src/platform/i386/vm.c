#include "types.h"
#include "printk.h"
#include "string.h"
#include "isr.h"
#include "mm.h"
#include "vm.h"
#include "task.h"

#define POSSIBLE_FRAMES 1024*1024

u32_t framebitmap[POSSIBLE_FRAMES/sizeof(u32_t)];

#define kerndir tasks[0].ptd
#define userdir tasks[1].ptd

pt_t kernel_pagetab[KERNEL_TABLES] __attribute__((aligned(4096)));
pt_t user_pagetab __attribute__((aligned(4096)));

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
    uintptr_t fault_addr;
    
    asm volatile("mov %%cr2, %0" : "=r" (fault_addr));

    die("Page Fault (%s%s%s%s) at 0x%X\n",
        !(regs->err_code & PAGE_P) ? "present " : "",
        regs->err_code & PAGE_RW ? "read-only " : "",
        regs->err_code & PAGE_US ? "user-mode " : "system ",
        regs->err_code & PAGE_PCD ? "reserved " : "",
        fault_addr);

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

    printk(INFO, "Adding free memory frames to bitmap\n");
    for (i = 0; i < POSSIBLE_FRAMES/sizeof(u32_t); i++) {
      framebitmap[i] = 0;
    }
    for (i = 0; i < POSSIBLE_FRAMES; i++) {
      framebitmap[i/(sizeof(u32_t)*8)] |= (i < KERNEL_TABLES * 1024 || i > memory_size / 4) ? 0 : 1 << i % (sizeof(u32_t)*8);
    }

    printk(INFO, "Registering page fault handler\n");
    register_interrupt_handler(14, &page_fault);

    printk(INFO, "Mapping pages to tables and directories\n");

    ptd_init(kerndir);

    for (i = 0; i <= KERNEL_TABLES; i++) {
      init_table(kernel_pagetab[i], (uint32_t*) (i * 4096 * 1024), PAGE_RW | PAGE_P | PAGE_G);
      ptd_map(kerndir, i, kernel_pagetab[i], PAGE_RW | PAGE_P | PAGE_G);
      ptd_map(userdir, i, kernel_pagetab[i], PAGE_RW | PAGE_P | PAGE_G);
    }

    ptd_map(kerndir, KERNEL_TABLES + 1, kernel_pagetab[0], PAGE_RW | PAGE_P);

    init_table(user_pagetab, (uint32_t*) (KERNEL_TABLES * 4096 * 1024), PAGE_RW | PAGE_P | PAGE_US);
    ptd_map(userdir, KERNEL_TABLES + 1, user_pagetab, PAGE_RW | PAGE_P | PAGE_US);

    printk(INFO, "Base user page is at %x\n", user_pagetab[0]);

    printk(INFO, "Loading page directory\n");
    ptd_load(kerndir);

    printk(INFO, "Enabling paging\n");
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    asm volatile("mov %0, %%cr0" : : "r"(cr0));
    printk (INFO, "OK\n");

    printk(INFO, "Finished\n");
}

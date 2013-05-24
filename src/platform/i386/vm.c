#include "types.h"
#include "printk.h"
#include "string.h"
#include "isr.h"
#include "mm.h"
#include "vm.h"

enum pagetable {
    PAGE_PRESENT        = 0x01,
    PAGE_WRITE          = 0x02,
    PAGE_USER           = 0x04,
    PAGE_WRITE_THROUGH  = 0x08,
    PAGE_CACHE_DISABLE  = 0x10
};


struct page_directory *kernel_directory = NULL;
struct page_directory *current_directory = NULL;

uint32_t *frames;
uint32_t nframes;

extern uintptr_t placement_address;

#define BITS 8
#define INDEX_FROM_BIT(a) (a / (BITS * sizeof(*frames)))
#define OFFSET_FROM_BIT(a) (a % (BITS * sizeof(*frames)))

static inline void
get_frame_idx_and_off(uint32_t f, uint32_t *idx, uint32_t *off)
{
    uint32_t frame = f/PAGE_SIZE;
    *idx = INDEX_FROM_BIT(frame);
    *off = OFFSET_FROM_BIT(frame);
}

static void
set_frame(uint32_t frame_addr)
{
    uint32_t idx, off;    
    get_frame_idx_and_off(frame_addr, &idx, &off);
    frames[idx] |= (0x1 << off);
}

#if 0
static void
clear_frame(uint32_t frame_addr)
{
    uint32_t idx, off;
    get_frame_idx_and_off(frame_addr, &idx, &off);
    
    frames[idx] &= ~(0x01 << off);
}

static uint32_t
test_frame(uint32_t frame_addr)
{
    uint32_t idx, off;
    get_frame_idx_and_off(frame_addr, &idx, &off);
    
    return frames[idx] & (0x01 << off);
}
#endif

static uint32_t
first_free_frame(void)
{
    uint32_t i, j;
    uint32_t t;
    
    for (i = 0; i < INDEX_FROM_BIT(nframes); ++i) {  
        
        // Frame full move along
        if (frames[i] == (uint32_t)-1)
            continue;

        for (j = 0; j < 32; ++j) {
            t = 0x1 << j; 
            if ((frames[i] & t) == 0)
                return i * 0x20 + j;
        }
    }

    /* UM ERROR? */
    printk(ERROR, "Returned bad first_free_frame\n");
    return -1;
}

#if 0
static void
free_frame(page_t *page)
{
   if (!page->frame)
       return;
  
   clear_frame(page->frame);
   page->frame = 0x0;
}
#endif

static int
alloc_frame(page_t *page, int is_kernel, int is_writable)
{
    uint32_t index;
    if (page->frame) {
        printk(ERROR, "FRAME IS Not Free 0x%X\n", page->frame);
        page->present = 1;
        page->rw = is_writable ? 1 : 0;
        page->user = is_kernel ? 0 : 1;
        return -1;
    }

    index = first_free_frame();
    if (index == (uint32_t)-1)
        die("ERROR KERNEL PANIC NO MEM\n");

    set_frame(index * PAGE_SIZE);
    page->present = 1;
    page->rw = is_writable ? 1 : 0;
    page->user = is_kernel ? 0 : 1;
    page->frame = index;

    return 0;
}

static page_t *
get_page(uint32_t address, int make, struct page_directory *dir)
{
    size_t table_idx;
    size_t tmp;

    address /= PAGE_SIZE;    
    table_idx = address / 1024;

    if (dir->tables[table_idx])
        return &dir->tables[table_idx]->pages[address % 1024];

    if (!make) {
        printk(INFO, "Not make return 0\n");
        return 0;
    }
    
    dir->tables[table_idx] = kmalloc_ap(sizeof(struct page_table), &tmp);
    memset(dir->tables[table_idx], 0, sizeof(struct page_table));
    dir->tables_physical[table_idx] = tmp | PAGE_PRESENT | PAGE_WRITE | PAGE_USER;

    return &dir->tables[table_idx]->pages[address % 1024];
}


static void
switch_page_directory(struct page_directory *dir)
{
    size_t cr0;
    
    /* set cirrent directory */
    current_directory = dir;
    asm volatile("mov %0, %%cr3" : : "r"(&dir->tables_physical));
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    asm volatile("mov %0, %%cr0" : : "r"(cr0));
}

static void
page_fault(struct registers *regs)
{
    uintptr_t fault_addr;
    
    asm volatile("mov %%cr2, %0" : "=r" (fault_addr));

    die("Page Fault ( %s%s%s%s) at 0x%X\n",
        !(regs->err_code & PAGE_PRESENT) ? "present " : "",
        regs->err_code & PAGE_WRITE ? "read-only " : "",
        regs->err_code & PAGE_USER ? "user-mode " : "",
        regs->err_code & PAGE_CACHE_DISABLE ? "reserved " : "",
        fault_addr);

}

void
paging__init(size_t memory_size)
{
    uintptr_t i;

    printk(INFO, "Intialize paging\n");
  
    printk(INFO, "MEMORY_SIZE: %dMB\n", memory_size/1024);
    nframes = memory_size / 4;//8; //PAGE_SIZE;
    printk(INFO, "%d number of frames\n", nframes);
    
    frames = kmalloc(INDEX_FROM_BIT(nframes));// * 8));
    memset(frames, 0, INDEX_FROM_BIT(nframes));

    kernel_directory = kmalloc_a(sizeof(struct page_directory));
    memset(kernel_directory, 0, sizeof(struct page_directory));
    
    for (i = 0; i < placement_address; i += PAGE_SIZE)
        if (alloc_frame(get_page(i, 1, kernel_directory), 0, 0) < 0)
            die("Getting page for address 0x%X failed\n", i);
    
    printk(INFO, "Registering handler\n");
    register_interrupt_handler(14, &page_fault);

    printk(INFO, "Switch page directory\n");
    switch_page_directory(kernel_directory);
    
    printk(INFO, "Finished\n");
}

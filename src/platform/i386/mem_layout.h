#ifndef MEM_LAYOUT_H
#define MEM_LAYOUT_H

#include <shared/cos_types.h>
#include <shared/consts.h>
#include <shared/cos_config.h>
#include <retype_tbl.h>

#include "multiboot.h"

/*
 * Virtual memory layout of the system.  Physical addresses can be
 * obtained with chal_va2pa.  See kernel.c for a set of assertions
 * that ensure the proper alignment of all memory sections.
 *
 * mem_X_{end,start}, where X is:
 * - kern  (K): the actual kernel image
 * - bootc (B): the memory for the booter component
 * - boot  (A): memory that is dynamically allocated at boot time (kernel bump allocator)
 * - kmem:      all kernel memory at boot-up time represented in the retype table
 * - utmem (U): memory that is untyped, and used by the components
 *
 * The memory image (physical memory):
 *
 * |-------|KKKKKKKK|WWWWW|BBBBBBBB|AA|UUUUUUUUUUUUUUUUUUU|
 *
 * |      ||                                              |
 * +---+--++---------------------+------------------------+
 *     |                         |
 * 1MB: x86 reserved            kmem
 *
 * For now, GRUB wastes a fair amount of memory between the kernel
 * image and the module, notated as W.
 */

struct mem_layout {
	u8_t *kern_end, *mod_start, *mod_end, *bootc_entry, *bootc_vaddr, *kern_boot_heap, *kmem_end;
	int   allocs_avail;
};
extern struct mem_layout glb_memlayout;

static inline u8_t *
mem_kern_start(void)
{
	return (void *)(COS_MEM_KERN_START_VA + COS_MEM_KERN_PA);
}
static inline u8_t *
mem_kern_end(void)
{
	return glb_memlayout.kern_end;
}
static inline u8_t *
mem_bootc_start(void)
{
	return glb_memlayout.mod_start;
}
static inline u8_t *
mem_bootc_end(void)
{
	return glb_memlayout.mod_end;
}
static inline u8_t *
mem_bootc_entry(void)
{
	return glb_memlayout.bootc_entry;
}
static inline u8_t *
mem_bootc_vaddr(void)
{
	return glb_memlayout.bootc_vaddr;
}
static inline u8_t *
mem_boot_start(void)
{
	return (u8_t *)round_up_to_pow2(mem_bootc_end(), RETYPE_MEM_NPAGES * PAGE_SIZE);
}
static inline u8_t *
mem_boot_end(void)
{
	return (u8_t *)round_up_to_pow2(glb_memlayout.kern_boot_heap, RETYPE_MEM_NPAGES * PAGE_SIZE);
}
/* what will the end be _after_ n allocations? */
static inline u8_t *
mem_boot_nalloc_end(int n)
{
	return (u8_t *)round_up_to_pow2(glb_memlayout.kern_boot_heap + n * PAGE_SIZE, RETYPE_MEM_NPAGES * PAGE_SIZE);
}
static inline u8_t *
mem_kmem_start(void)
{
	return mem_bootc_start();
}
static inline u8_t *
mem_kmem_end(void)
{
	return glb_memlayout.kmem_end;
}
/* not that this is only valid _after_ all boot-time kernel allocations are made */
static inline u8_t *
mem_utmem_start(void)
{
	return mem_boot_end();
}
static inline u8_t *
mem_utmem_end(void)
{
	return mem_kmem_end();
}
u8_t *mem_boot_alloc(int npages); /* boot-time, bump-ptr heap */

#endif /* MEM_LAYOUT_H */

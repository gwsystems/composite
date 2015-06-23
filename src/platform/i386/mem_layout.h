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
 * - kern: the actual kernel image
 * - bootc: the memory for the booter component
 * - boot: memory that can be used at boot time for allocations
 * - kmem: memory that can be retyped normally (including bootc memory)
 * - usermem: memory that is user virtual memory and cannot be retyped
 */

struct mem_layout {
	u8_t *kern_end, *mod_start, *mod_end, *bootc_entry, *bootc_vaddr, *kern_boot_heap;
	int allocs_avail;
};
extern struct mem_layout glb_memlayout;

static inline u8_t *mem_kern_start(void)    { return (void*)(COS_MEM_KERN_START_VA+COS_MEM_KERN_PA); }
static inline u8_t *mem_kern_end(void)      { return glb_memlayout.kern_end; }
static inline u8_t *mem_bootc_start(void)   { return glb_memlayout.mod_start; }
static inline u8_t *mem_bootc_end(void)     { return glb_memlayout.mod_end; }
static inline u8_t *mem_bootc_entry(void)   { return glb_memlayout.bootc_entry; }
static inline u8_t *mem_bootc_vaddr(void)   { return glb_memlayout.bootc_vaddr; }
static inline u8_t *mem_boot_start(void)    { return (u8_t*)round_up_to_pow2(mem_bootc_end(), RETYPE_MEM_NPAGES * PAGE_SIZE); }
static inline u8_t *mem_boot_end(void)      { return (u8_t*)round_up_to_pow2(glb_memlayout.kern_boot_heap, RETYPE_MEM_NPAGES); }
static inline u8_t *mem_boot_nalloc_end(int n) { return (u8_t*)round_up_to_pow2(glb_memlayout.kern_boot_heap + n*PAGE_SIZE, RETYPE_MEM_NPAGES); }
static inline u8_t *mem_kmem_start(void)    { return mem_bootc_start(); }
static inline u8_t *mem_kmem_end(void)      { return mem_kmem_start() + COS_MEM_KERN_PA_SZ; }
static inline u8_t *mem_usermem_start(void) { return (u8_t *)chal_pa2va(COS_MEM_USER_PA);}
static inline u8_t *mem_usermem_end(void)   { return mem_usermem_start() + COS_MEM_USER_PA_SZ; }
u8_t *mem_boot_alloc(int npages); /* boot-time, bump-ptr heap */

#endif	/* MEM_LAYOUT_H */

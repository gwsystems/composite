#ifndef MEM_LAYOUT_H
#define MEM_LAYOUT_H

#include <shared/cos_types.h>
#include <shared/consts.h>
#include <shared/cos_config.h>

#include "multiboot.h"

/* 
 * Virtual memory layout of the system.  Physical addresses can be obtained with chal_va2pa.
 * 
 * [COS_MEM_KERN_START_VA, mem_kern_start)            : platform reserved 
 * [mem_kern_start, mem_kern_end)                     : kernel image
 * [mem_kern_end<=mem_bootc_start, mem_bootc_end)     : boot component image
 * [mem_bootc_end<=mem_boot_start, mem_boot_end)      : heap for the boot process (to allocate ptes)
 * [mem_boot_start<=mem_kmem_start, mem_kmem_end)     : kernel-accessible memory (overlap w/ heap)
 * [mem_kmem_end<=mem_usermem_start, mem_usermem_end) : kernel-inaccessible memory (permanently typed as user memory)
 */

struct mem_layout {
	void *mod_start, *mod_end, *kern_boot_heap;
};
extern struct mem_layout glb_memlayout;
extern void *end; 		/* from the linker script */

static inline void *mem_kern_start(void)  { return (void*)(COS_MEM_KERN_START_VA+COS_MEM_KERN_PA); }
static inline void *mem_kern_end(void)    { return end; }
static inline void *mem_bootc_start(void) { return glb_memlayout.mod_start; }
static inline void *mem_bootc_end(void)   { return glb_memlayout.mod_end; }
static inline void *mem_boot_start(void)  { return (void*)round_up_to_pow2(mem_bootc_end(), RETYPE_MEM_NPAGES * PAGE_SIZE); }
static inline void *mem_boot_end(void)    { return glb_memlayout.kern_boot_heap; }
static inline void *mem_kmem_start(void)  { return mem_boot_start(); }
static inline void *mem_kmem_end(void)    { return mem_kmem_start() + COS_MEM_KERN_PA_SZ; }
static inline void *mem_usermem_start(void) { return (void *)chal_pa2va(COS_MEM_USER_PA);}
static inline void *mem_usermem_end(void) { return mem_usermem_start() + COS_MEM_USER_PA_SZ; }

#endif	/* MEM_LAYOUT_H */

/*
 * This file is a little bit of a mess currently, and is a dumping
 * grounds for x86_64-specific configuration values.
 */

#pragma once

#define ENABLE_SERIAL        1

/*
 * 1 MB, note that this is not the PA of kernel-usable memory, instead
 * it is the PA of the kernel.  If you change this, update the kernel
 * linker script (.ld) as well.
 */
#define COS_MEM_KERN_PA (0x00100000)
#define COS_MEM_KERN_PA_ORDER (33)
#define COS_MEM_KERN_PA_SZ ((1UL << COS_MEM_KERN_PA_ORDER) - (1UL << 26)) /* FIXME: Need a way to get physical memory size from kernel. Cannot use a hardcoded value, actual memory could be much lower! */

#define COS_MEM_COMP_START_VA ((1 << 30) + (1 << 22)) /* 1GB + 4MB (a relic) */

#define COS_MEM_KERN_HIGH_ADDR_VA_PGD_MASK 0x0000ff8000000000
#define COS_MEM_KERN_START_VA (0xffff800000000000) // COS_MEM_KERN_PA     /* currently, we don't do kernel relocation */
#define COS_MEM_USER_MAX_VA 0x00007fffffffffff

#define COS_HW_MMIO_MAX_SZ (1UL << 27) /* Assuming a MAX of 128MB for MMIO. */
#define COS_PHYMEM_MAX_SZ ((1UL << 35) - (1UL << 22) - COS_HW_MMIO_MAX_SZ) /* 1GB - 4MB - MMIO sz */
#define COS_PHYMEM_END_PA ((1UL << 35) - COS_HW_MMIO_MAX_SZ) /* Maximum usable physical memory */

/* the CPU that does initialization for Composite */
#define INIT_CORE 0

/* Composite user memory uses physical memory above this. */
#define COS_MEM_START COS_MEM_KERN_PA

/* from chal/shared/chal_consts.h */
#define PAGE_MASK (~(PAGE_SIZE - 1))
#define PGD_SHIFT 39
#define PGD_RANGE (1UL << PGD_SHIFT)
#define PGD_SIZE PGD_RANGE
#define PGD_MASK (~(PGD_RANGE - 1))
#define PGD_PER_PTBL 512

#define PGT1_SHIFT 30
#define PGT1_RANGE (1UL << PGT1_SHIFT)
#define PGT1_SIZE PGT1_RANGE
#define PGT1_MASK (~(PGT1_RANGE - 1))
#define PGT1_PER_PTBL 512

#define PGT2_SHIFT 21
#define PGT2_RANGE (1UL << PGT2_SHIFT)
#define PGT2_SIZE PGT2_RANGE
#define PGT2_MASK (~(PGT2_RANGE - 1))
#define PGT2_PER_PTBL 512

#define PGT3_SHIFT 12
#define PGT3_RANGE (1UL << PGT3_SHIFT)
#define PGT3_SIZE PGT3_RANGE
#define PGT3_MASK (~(PGT3_RANGE - 1))
#define PGT3_PER_PTBL 512

/* root page tbale is 0, then second level page table is 1, etc.*/
#define COS_PGTBL_DEPTH 4
#define COS_PGTBL_ORDER_PTE_3 12
#define COS_PGTBL_ORDER_PTE_2 21
#define COS_PGTBL_ORDER_PTE_1 30
#define COS_PGTBL_ORDER_PTE_0 39

/* PTE user pagetable modifiable flags */
#define COS_PAGE_READABLE (0)
#define COS_PAGE_WRITABLE (1ul << 1)
#define COS_PAGE_PKEY0    (1ul << 59)
#define COS_PAGE_PKEY1    (1ul << 60)
#define COS_PAGE_PKEY2    (1ul << 61)
#define COS_PAGE_PKEY3    (1ul << 62)
#define COS_PAGE_XDISABLE (1ul << 63)

#ifndef TYPES_H
#define TYPES_H

#define MNULL ((void*)0)

/* 
 * These types are for addresses that are never meant to be
 * dereferenced.  They will generally be used to set up page table
 * entries.
 */
typedef unsigned long phys_addr_t;
typedef unsigned long vaddr_t;
typedef unsigned int page_index_t;

#define COS_BRAND_CREATE  0x1
#define COS_BRAND_ADD_THD 0X2

#define IL_INV_UNMAP (0x1) // when invoking, should we be unmapped?
#define IL_RET_UNMAP (0x2) // when returning, should we unmap?
#define MAX_ISOLATION_LVL_VAL (IL_INV_UNMAP|IL_RET_UNMAP)

/*
 * Note on Symmetric Trust, Symmetric Distruct, and Asym trust: 
 * ST iff (flags & (CAP_INV_UNMAP|CAP_RET_UNMAP) == 0)
 * SDT iff (flags & CAP_INV_UNMAP && flags & CAP_RET_UNMAP)
 * AST iff (!(flags & CAP_INV_UNMAP) && flags & CAP_RET_UNMAP)
 */
#define IL_ST  (0)
#define IL_SDT (IL_INV_UNMAP|IL_RET_UNMAP)
#define IL_AST (IL_RET_UNMAP)
/* invalid type, can NOT be used in data structures, only for return values. */
#define IL_INV (~0) 
typedef unsigned int isolation_level_t;

#define CAP_SAVE_REGS 0x1

#ifdef __KERNEL__
#include <asm/atomic.h>
#else

typedef struct { volatile unsigned int counter; } atomic_t;

#endif /* __KERNEL__ */

#endif /* TYPES_H */

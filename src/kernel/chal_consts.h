#pragma once

#define COS_NUM_CPU               1

#define COS_NUM_REGS              16

#define COS_PAGE_SIZE             4096
#define COS_CACHELINE_SIZE        64
#define COS_COHERENCE_UNIT_SIZE   128
#define COS_WORD_SIZE             8

#define COS_PGTBL_TOP_NENT        256
#define COS_PGTBL_TOP_BITS        8
#define COS_PGTBL_KERN_NENT       256
#define COS_PGTBL_INTERNAL_NENT   512
#define COS_PGTBL_INTERNAL_BITS   9
#define COS_PGTBL_PERM_PRESENT    1
#define COS_PGTBL_PERM_WRITE      2
#define COS_PGTBL_PERM_EXECUTE    4
#define COS_PGTBL_PERM_USER       8
#define COS_PGTBL_PERM_UNTYPED    16
#define COS_PGTBL_PERM_RESERVED   32
#define COS_PGTBL_PERM_KERNEL     64
/* Aggregate permissions: */
#define COS_PGTBL_PERM_VM_REQ     9     /* 1 + 8 */
#define COS_PGTBL_PERM_VM_ALLOWED 15    /* 1 + 2 + 4 + 8 */
#define COS_PGTBL_PERM_ALLOCATED  97	/* 64 + 32 + 1 */
#define COS_PGTBL_DEFAULT_INTERN_PERM 1 /* internal nodes */
#define COS_PGTBL_ALLOWED_INTERN_PERM 1
#define COS_PGTBL_PERM_MASK       4095 /* all 1s up to the address */
#define COS_PGTBL_MAX_DEPTH       4

#define COS_PAGE_KERNTYPE_PGTBL_LEAF 7 /* TODO update when it is updated in the cos_consts.h */

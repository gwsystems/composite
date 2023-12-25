/*
 * Do NOT include this file. Instead, include cos_consts.h.
 */

#pragma once

#define COS_PAGE_ORDER            12
#define COS_PAGE_SIZE             4096
#define COS_CACHELINE_SIZE        64
#define COS_COHERENCE_UNIT_SIZE   128
#define COS_WORD_SIZE             8

#define COS_PGTBL_TOP_NENT        256
#define COS_PGTBL_TOP_ORD         8
#define COS_PGTBL_KERN_NENT       256
#define COS_PGTBL_INTERNAL_NENT   512
#define COS_PGTBL_LEAF_NENT       512
#define COS_PGTBL_INTERNAL_ORD    9
#define COS_PGTBL_LEAF_ORD        9
#define COS_PGTBL_PERM_PRESENT    1
#define COS_PGTBL_PERM_WRITE      2
#define COS_PGTBL_PERM_EXECUTE    4
#define COS_PGTBL_PERM_USER       8
/* Aggregate permissions: */
#define COS_PGTBL_PERM_VM_RO          5 /* present, RO, user-level */
#define COS_PGTBL_PERM_VM_EXEC        5 /* Ignoring bit 64 for now */
#define COS_PGTBL_PERM_VM_RW          7
#define COS_PGTBL_PERM_VM_REQ         5
#define COS_PGTBL_PERM_VM_ALLOWED     7 /* present, RW, user-level */
#define COS_PGTBL_PERM_KERN           0
#define COS_PGTBL_PERM_INTERN_DEFAULT 7 /* internal nodes -- present, RW, user-level */
#define COS_PGTBL_PERM_INTERN_ALLOWED 7
#define COS_PGTBL_PERM_INTERN_REQ     5	/* present, RO, user-level */
#define COS_PGTBL_PERM_MASK           4095 /* all 1s up to the address */
#define COS_PGTBL_MAX_DEPTH           4

/*
 * FIXME: does not consider the 64th bit in page-table entries for
 * W^X, or 1 if execute disabled.
 */

#pragma once

#include <chal_consts.h>
#include <chal_types.h>

/*
 * Shared Kernel/User Constants and Types
 */

/*
 * The different capability types, that map to the operations that can
 * be performed on them. These populate variables with the
 * `cap_type_t` type.
 */
#define COS_CAP_TYPE_FREE         0  /* Unused slot, can be allocated */
#define COS_CAP_TYPE_RESERVED     1  /* Entry has an update in progress */
#define COS_CAP_TYPE_SINV         2
#define COS_CAP_TYPE_THD          3
#define COS_CAP_TYPE_CAPTBL_0     4
#define COS_CAP_TYPE_CAPTBL_1     5
#define COS_CAP_TYPE_CAPTBL_LEAF  5
#define COS_CAP_TYPE_PGTBL_0      6
#define COS_CAP_TYPE_PGTBL_1      7
#define COS_CAP_TYPE_PGTBL_2      8
#define COS_CAP_TYPE_PGTBL_3      9
#define COS_CAP_TYPE_PGTBL_LEAF   9
#define COS_CAP_TYPE_COMP         10
#define COS_CAP_TYPE_HW           11
#define COS_CAP_TYPE_HWVM         12 /* Hardware-specific virtualization support page (e.g. VMCB for VTx) */
#define COS_CAP_TYPE_SCB          13 /* Scheduling control block shared with user-level */
#define COS_CAP_TYPE_DCB          14 /* Dispatch control block shared with user-level */
#define COS_CAP_TYPE_ICB          15 /* Invocation control block shared with user-level */
#define COS_CAP_TYPE_RTCB         16 /* Resource table control block shared with user-level */
#define COS_CAP_TYPE_VMCB         17 /* Virtual Machine context control block shared with user-level */
#define COS_CAP_TYPE_NIL          18 /* Empty slot that cannot be modified */
#define COS_CAP_TYPE_NUM          19

/**
 * The COS_OP_* values are operations that can be performed on
 * capabilities of different types. These populate variables that have
 * the `op_bitmap_t` type. A "main" capability must have these
 * operation permissions to allow the operation to be performed on it.
 * Additionally, if multiple capabilities are targeted by an operation
 * (i.e. creating a component requires targeting a capability-table
 * capability and a page-table capability), then the same permission
 * is required for those capabilities in addition to the main one.
 *
 * Note that there is no operation code for a synchronous invocation
 * or return, as these are the sole operation that can be performed on
 * a synchronous invocation capability, thus making the operation code
 * superfluous.
 */

#define COS_OP_NIL                      0
#define COS_OP_NEST                     1
#define COS_OP_INTROSPECT               2
#define COS_OP_RESTBL_CONSTRUCT         4
#define COS_OP_RESTBL_DECONSTRUCT       8
#define COS_OP_PGTBL_RETYPE_PGTBL       16
#define COS_OP_PGTBL_RETYPE_CAPTBL      32
#define COS_OP_PGTBL_RETYPE_THD         64
#define COS_OP_PGTBL_RETYPE_COMP        128
#define COS_OP_PGTBL_RETYPE_DEALLOCATE  256
#define COS_OP_CAPTBL_CAP_CREATE_RESTBL 512
#define COS_OP_CAPTBL_CAP_CREATE_THD    1024
#define COS_OP_CAPTBL_CAP_CREATE_SINV   2048
#define COS_OP_CAPTBL_CAP_CREATE_COMP   4096
#define COS_OP_CAPTBL_CAP_REMOVE        8192
#define COS_OP_RESTBL_CAP_COPY          16384
#define COS_OP_THD_DISPATCH             32768
#define COS_OP_THD_EVT_OR_DISPATCH      65536
#define COS_OP_THD_AWAIT_EVT            131072
#define COS_OP_THD_TRIGGER_EVT          262144
#define COS_OP_THD_CALL                 524288
#define COS_OP_THD_REPLY_WAIT           1048576
#define COS_OP_THD_SCHEDULE             2097152
#define COS_OP_HW                       4194304
#define COS_OP_CAP_COPY                 8388608
#define COS_OP_ALL                      16777215

/*
 * - captbl cons/decons: target = {pg|cap}tbl & op == (COS_OP_MODIFY_{ADD|REMOVE} | COS_OP_RESTBL_CONSTRUCT)
 * - retype to/from untyped: target = pgtbl leaf & op == COS_OP_RETYPE_{CREATE|DEALLOCATE}
 * - captbl slot create (from resource and/or other cap slot): target = captbl leaf &
 *   op == COS_OP_MODIFY_ADD, target2 = pgtbl leaf & op == COS_OP_CAP_COPY
 * - captbl slot copy/pgtbl slot copy: target = pgtbl, target2 = pgtbl, or both captbl
 *   op = COS_OP_CAP_COPY (for restbl & slot) & op2 = COS_OP_MODIFY_ADD (for restbl)
 * - captbl
 * - thread ops
 * - sinv/sret
 */

/* Memory constants */
#define COS_NUM_RETYPEABLE_PAGES  4
#define COS_NUM_VM_PAGES          0

/*
 * Page types, including separate types for different levels in the
 * resource tables. The first eight bits are the high-level type. The
 * remaining types describe the kind of kernel type (if the high-level
 * type is "kernel").
 */
#define COS_PAGE_TYPE_UNTYPED     0
#define COS_PAGE_TYPE_RETYPING    1   /* Memory that is being prepared to be a kernel structure */
#define COS_PAGE_TYPE_VM          2   /* User-level Virtual Memory */
#define COS_PAGE_TYPE_KERNEL      3   /* Kernel type. The specific "flavor" is selected in the following... */

#define COS_PAGE_TYPE_BITS        8   /* How many bits hold the type? */
#define COS_PAGE_TYPE_BITS_MASK   255 /* Mask for those bits. */
/* Packed into bits past the first two, the specific kernel type: */
#define COS_PAGE_KERNTYPE_THD         1   /* A thread including a tcap */
#define COS_PAGE_KERNTYPE_CAPTBL_0    2   /* Capability-table nodes at different levels... */
#define COS_PAGE_KERNTYPE_CAPTBL_1    3
#define COS_PAGE_KERNTYPE_CAPTBL_LEAF 3
#define COS_PAGE_KERNTYPE_PGTBL_0     4   /* Page-table nodes at different levels... */
#define COS_PAGE_KERNTYPE_PGTBL_1     5
#define COS_PAGE_KERNTYPE_PGTBL_2     6
#define COS_PAGE_KERNTYPE_PGTBL_3     7
#define COS_PAGE_KERNTYPE_PGTBL_LEAF  7
#define COS_PAGE_KERNTYPE_COMP        8
#define COS_PAGE_KERNTYPE_HW          9   /* Hardware/platform specific resources */
#define COS_PAGE_KERNTYPE_HWVM        10
#define COS_PAGE_KERNTYPE_SCB         11  /* Control blocks shared between user and kernel for... */
#define COS_PAGE_KERNTYPE_DCB         12  /* ...the scheduler, thread dispatching, invocations... */
#define COS_PAGE_KERNTYPE_ICB         13  /* ...invocation control block when invocations use bypass... */
#define COS_PAGE_KERNTYPE_RTCB        14  /* ...resource-table/memory, and... */
#define COS_PAGE_KERNTYPE_VMCB        15  /* ...virtualization. */
#define COS_PAGE_KERNTYPE_NUM         16  /* The number of page types */

#define COS_EPOCH_BIT_LIMIT       63
#define COS_REFCNT_BIT_LIMIT      63

/*
 * Resource tables include the capability- and page-tables which are
 * both radix tries. These defines select the radix trie sizes (i.e.
 * how many indices do they have, and how deep are they).
 */
#define COS_CAPTBL_INTERNAL_NENT 512  /* (COS_PAGE_SIZE / sizeof(captbl_t)) */
#define COS_CAPTBL_INTERNAL_ORD    9  /* log_2(COS_CAPTBL_INTERNAL_NENT) */
#define COS_CAPTBL_LEAF_NENT      64  /* (COS_PAGE_SIZE / sizeof(struct capability_generic)) */
#define COS_CAPTBL_LEAF_ORD        6  /* log_2(COS_CAPTBL_LEAF_NENT) */
#define COS_CAPTBL_LEAF_ENTRY_SZ  64  /* sizeof(struct capability_generic) */
#define COS_CAPTBL_MAX_DEPTH       2

/**
 * Thread states that can be reported to the scheduler
 */
#define COS_THD_STATE_NULL           0   /* only used as a user-level retval to denote "nothing" */
#define COS_THD_STATE_EVT_AWAITING   1   /* after calling await_asnd */
#define COS_THD_STATE_EXECUTING      2   /* normal state, executable, potentially event-triggered */
#define COS_THD_STATE_IPC_DEPENDENCY 3   /* IPC dependency on another thread */
#define COS_THD_STATE_IPC_AWAIT      4   /* awaiting IPC from another thread */
#define COS_THD_STATE_SCHED_AWAIT    5   /* scheduler awaiting event */
#define COS_THD_STATE_EVT_TRIGGERED  6   /* after calling await_asnd */

#define COS_ASSERTIONS_ACTIVE 1

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
#define COS_CAP_TYPE_PGTBL_0      4
#define COS_CAP_TYPE_PGTBL_1      5
#define COS_CAP_TYPE_PGTBL_2      6
#define COS_CAP_TYPE_PGTBL_3      7
#define COS_CAP_TYPE_PGTBL_LEAF   7
#define COS_CAP_TYPE_CAPTBL_0     8
#define COS_CAP_TYPE_CAPTBL_1     9
#define COS_CAP_TYPE_CAPTBL_LEAF  9
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

/*
 * The operations that can be performed on capabilities of different
 * types. These populate variables that have the `op_bitmap_t` type.
 */

/* All operations can be performed on nested capability tables */
#define COS_OP_NIL                0    /* No permissions */
#define COS_OP_NEST               1    /* Look up the capability in the nested capability table. */
#define COS_OP_INTROSPECT         2    /* Introspect to get information about the capability/resource */
#define COS_OP_CAP_COPY           4    /* Can we copy a given capability */
#define COS_OP_CAP_REMOVE         8    /* Can we remove a given capability */
#define COS_OP_MODIFY_ADD         16   /* Can we modify the resource by adding to it? */
#define COS_OP_MODIFY_REMOVE      32   /* Can we modify the resource by removing references from it? */
#define COS_OP_MODIFY_UPDATE      64   /* Can we modify the resource by updating its existing state? */
#define COS_OP_CONSTRUCT          128  /* Can we use the resource in the construction of others? */
#define COS_OP_DEALLOCATE         256  /* Can we deallocate the resource? */

/*
 * If the capability type is "thread", these ops apply. Dispatch and
 * await can have event passed as an option, and the thread will only
 * await/dispatch if there is no pending event. Otherwise, and event
 * is returned. Await will only happen if we activate a thread
 * capability to ourself, and dispatching only has an effect if
 * another thread's capability is activated.
 */
#define COS_OP_THD_AWAIT_EVENT    512   /* The current thread (also targeted by the capability), awaits an activation, retrieves an event */
#define COS_OP_THD_DISPATCH       1024  /* Switch to the target thread. */
#define COS_OP_THD_CALL           2048  /* Switch to the thread, creating a dependency between the current, and the target thread. */
#define COS_OP_THD_REPLY_WAIT     4096  /* Reply to a CALL, and wait for the next call. */
#define COS_OP_THD_IGNORE_PRIO    8192  /* Avoid using TCap priority to determine if we should switch to this thread. */
#define COS_OP_THD_TRIGGER_EVENT  16384 /* Trigger an asynchronous event. */
#define COS_OP_THD_TIME_TRANSFER  32768 /* Enable time to be transferred into the target thread's TCap. */
#define COS_OP_THD_TIMER_PROGRAM  65536 /* Enable the timer to be programmed when dispatching. */

#define COS_OP_HW_PHYS_MEM_MAP    512
#define COS_OP_HW_INTERRUPT_BIND  1024
#define COS_OP_HW_SERIAL_OUTPUT   2048

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
#define COS_PAGE_KERNTYPE_SCB         8   /* Control blocks shared between user and kernel for... */
#define COS_PAGE_KERNTYPE_DCB         9   /* ...the scheduler, thread dispatching, invocations... */
#define COS_PAGE_KERNTYPE_ICB         10  /* ...invocation control block when invocations use bypass... */
#define COS_PAGE_KERNTYPE_RTCB        11  /* ...resource-table/memory, and... */
#define COS_PAGE_KERNTYPE_VMCB        12  /* ...virtualization. */
#define COS_PAGE_KERNTYPE_HWVM        13
#define COS_PAGE_KERNTYPE_COMP        14
#define COS_PAGE_KERNTYPE_HW          15  /* Hardware/platform specific resources */
#define COS_PAGE_KERNTYPE_NUM         16  /* The number of page types */

#define COS_EPOCH_BIT_LIMIT       63
#define COS_REFCNT_BIT_LIMIT      63

/*
 * Resource tables include the capability- and page-tables which are
 * both radix tries. These defines select the radix trie sizes (i.e.
 * how many indices do they have, and how deep are they).
 */
#define COS_CAPTBL_INTERNAL_NENT  512 /* (COS_PAGE_SIZE / sizeof(captbl_t)) */
#define COS_CAPTBL_LEAF_NENT      64  /* (COS_PAGE_SIZE / 64) */
#define COS_CAPTBL_LEAF_ENTRY_SZ  64  /* (COS_PAGE_SIZE / 64) */
#define COS_CAPTBL_MAX_DEPTH      2

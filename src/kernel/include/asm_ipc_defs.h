#ifndef ASM_IPC_DEFS_H
#define ASM_IPC_DEFS_H

/* Definitions */

/* the offset on the stack to the fn return address for trust cases */
//#define IPRETURN 12 // saving ecx & edx??
#define IPRETURN 4

/* offsets into the thd_invocation_frame structure */
#define SFRAMEUSR 4
#define SFRAMESP 8
#define SFRAMEIP 12

/* user capability structure offsets */
/* really 16, see below for use (mult index reg by 2) */
#define UCAP_SZ 3 /* # of longs */
#define UCAP_SZ_STR "3"
#define SIZEOFUSERCAP (UCAP_SZ * 4)
#define INVFN 0
#define CAPNUM 4
#define SERVER_FN 16 

/* offsets into syscall integer */
#define COS_ASYNC_CAP_FLAG_BIT 32 /* async cap flag -> 32 */
#define COS_ASYNC_CAP_FLAG (1 << (COS_ASYNC_CAP_FLAG_BIT - 1))
#define COS_CAPABILITY_OFFSET 16 /* bits 16->31 */
#define COS_SYSCALL_OFFSET 15    /* bits 15->20 */

#define RET_CAP (1 << COS_CAPABILITY_OFFSET)

/* We have sanity checks of the following defines when loading
 * Composite kernel module. */
#define CPUID_OFFSET_IN_THREAD_INFO (16)
#define THREAD_SIZE_LINUX (4096 * 2)
#define LINUX_THREAD_INFO_RESERVE (64 * 2)
#define LINUX_INFO_PAGE_MASK (~(THREAD_SIZE_LINUX - 1))

#endif /* ASM_IPC_DEFS_H */

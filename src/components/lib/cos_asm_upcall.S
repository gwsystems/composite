/**
 * Copyright 2007 by Boston University, Gabriel Parmer,
 * gabep1@cs.bu.edu, and The George Washington University,
 * Gabriel Parmer, gparmer@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#define __ASM__

#include "../../kernel/include/asm_ipc_defs.h"
#include <cos_asm_stacks.h>
#include <consts.h>

#define IPRETURN 4

#if ENABLE_STACK_MANAGER!=1
.section .initonce
.align 32
nil:
	.rep 1024
	.long 0
	.endr

.globl cos_static_stack
cos_static_stack:
	.rep ALL_STACK_SZ
	.long 0
	.endr
.globl cos_static_stack_end
cos_static_stack_end:
#endif

.section .initonce
.globl stkmgr_stack_space
stkmgr_stack_space:
	.rep ALL_TMP_STACKS_SZ
	.long 0
	.endr
	
.text
.globl cos_upcall_entry
.type  cos_upcall_entry, @function
.align 16
cos_upcall_entry:
	COS_ASM_GET_STACK

	xor %ebp, %ebp
	pushl %esi
	pushl %edi
	pushl %ebx
	pushl %ecx /* option */	
	call cos_upcall_fn
	addl $16, %esp

	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */
	COS_ASM_RET_STACK

	sysenter
	COS_ASM_REQUEST_STACK
/*
 * %eax = cmpval, %ebx = memaddr, %ecx = newval
 * output %edx, either cmpval: fail, or newval:	success
 */
.weak cos_atomic_cmpxchg
.type cos_atomic_cmpxchg, @function
.align 16
cos_atomic_cmpxchg:
	movl %eax, %edx   /* this resets edx if we rollback after XXX */
	cmpl (%ebx), %eax
	jne cos_atomic_cmpxchg_end
	movl %ecx, %edx   /* XXX */
	movl %ecx, (%ebx)
.weak cos_atomic_cmpxchg_end
cos_atomic_cmpxchg_end:	
	ret

/*
 * %eax = semaphore_addr, %ebx = thread_id, %ecx = count
 */
.weak cos_atomic_user1
.type cos_atomic_user1, @function
cos_atomic_user1:
.weak cos_atomic_user1_end
cos_atomic_user1_end:
.weak cos_atomic_user2
.type cos_atomic_user2, @function
cos_atomic_user2:
.weak cos_atomic_user2_end
cos_atomic_user2_end:
.weak cos_atomic_user3
.type cos_atomic_user3, @function
cos_atomic_user3:
.weak cos_atomic_user3_end
cos_atomic_user3_end:
.weak cos_atomic_user4
.type cos_atomic_user4, @function
cos_atomic_user4:
.weak cos_atomic_user4_end
cos_atomic_user4_end:
	/* crash out as something's wrong */
	movl $0, %eax
	movl (%eax), %eax


/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#define __ASM__
#include <consts.h>
#include <cos_asm_simple_stacks.h>
#include "../../kernel/include/asm_ipc_defs.h"

#define IPRETURN 4

.section .initonce
.align 32
.globl nil
nil:
	.rep 1024
	.long 0
	.endr

.align COS_STACK_SZ
.globl cos_static_stack
cos_static_stack:
	.rep ALL_STACK_SZ
	.long 0
	.endr
.globl cos_static_stack_end
cos_static_stack_end:

.text
.globl cos_upcall_entry
.type  cos_upcall_entry, @function
.align 16
cos_upcall_entry:
	COS_ASM_GET_STACK

	pushl $0
	movl %esp, -8(%esp)
	pushl $0
	movl %esp, -8(%esp)
	subl $8, %esp
	pushl %esi
	pushl %edi
	pushl %ebx
	xor %ebp, %ebp
	pushl %ecx /* option */
	call cos_upcall_fn
	addl $24, %esp

	popl %esi
	popl %edi

	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */
	COS_ASM_RET_STACK

	sysenter

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

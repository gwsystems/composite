/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <cos_asm_stacks.h>
	
#define RET_CAP ((1<<20)-1)
#define IPRETURN 4

.data
/* 16 8K stacks */
.align 32
.globl nil
nil:
/*	.rep 1024 */
	.rep 2048
	.long 0
	.endr
.globl cos_static_stack
cos_static_stack:
/*	.rep 8193*/
	.rep 16385
	.long 0
	.endr

/*.weak cos_sched_notifications
cos_sched_notifications:
	.long 0
	.long 0
	.long 0
	.long 0
*/
	
/* Scheduler page for event notifications and thread switching. */
/*.align 4096
.globl cos_sched_notifications
cos_sched_notifications:
	.rep 1024
	.long 0
	.endr
	*/
	
.text
.globl cos_upcall_entry
.type  cos_upcall_entry, @function
.align 16
cos_upcall_entry:
	COS_ASM_GET_STACK

	pushl %esi
	pushl %edi
	pushl %ebx
	pushl %ecx /* option */	
	call cos_upcall_fn
	/* addl $8, %esp <- dont need to clean the stack */

	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */

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
	nop

/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

.text
/*
 * cos_semaphore_take:	
 *
 * Bitwise format of a semaphore value:
 * | contested | thread id | generation | 
 * |     1     |    15     |     16     |
 *
 * This is too complicated for its own good.  When the kernel gets 
 * an interrupt and detects that a thread was executing in one of these 
 * sections, it will roll back the ip to the start.
 *
 * Can't modify the in regs as we might start over (rollback)
 * in:  %eax = semaphore_addr, %ebx = curr_thread_id
 * out:	%edi = holding thread, %ecx = generation, %edx = new semaphore value
 */
.globl cos_atomic_user1
.type cos_atomic_user1, @function
.align 16
cos_atomic_user1:
	movl (%eax), %ecx

	/* get the current holding thread */
	movl %ecx, %edi
	shr  $16,  %edi

	/* get the current generation */
	andl $0x0000FFFF, %ecx

	cmpl $0, %edi
	jne  locked
	/* manipulate the thread id to be in the correct bitwise position */
	movl %ebx, %edx
	jmp  commit
locked:
	movl %edi, %edx
	orl  $0x8000, %edx /* set the contended bit */
commit:	 
	shl  $16,  %edx         /* offset current thread id, assume that the top bit is 0 (for uncontested) */
	orl  %ecx, %edx
	/* commit the new semaphore value: same generation, new thread */
	movl %edx, (%eax)
.globl cos_atomic_user1_end
cos_atomic_user1_end:
	ret
		
/*
 * cos_semaphore_release:	
 *
 * in:  %eax = semaphore addr, %ebx = curr_thd_id
 * out: %ecx = new sem value (generation), %edi = last blocked thd 
 */
.globl cos_atomic_user2 
.type cos_atomic_user2, @function
.align 16
cos_atomic_user2:
	movl (%eax), %ecx

	/* get the contention bit */
	movl %ecx, %edi
	shr  $31, %edi

	/* get the current generation */
	andl $0x0000FFFF, %ecx 

	/* is the contested flag set? */
	cmpl $0, %edi
	je   no_contention

	/* if contention, update generation */
	inc  %cx
no_contention:
	/* commit the new semaphore value */
	movl %ecx, (%eax) /* any path should NULL out the owning thread */
.globl cos_atomic_user2_end
cos_atomic_user2_end:
	ret

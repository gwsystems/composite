.text
/*
 * cos_semaphore_take:	
 *
 * Bitwise format of a semaphore value:
 * | thread id | generation | 
 * |    16     |     16     |
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
	movl *%eax, %ecx

	/* get the current holding thread */
	movl %ecx, %edi
	shr  $16,  %edi

	/* get the current generation */
	andl $0000FFFF, %ecx

	/* manipulate the thread id to be in the correct bitwise position */
	movl %ebx, %edx
	shl  $16,  %edx         /* offset current thread id */
	orl  %ecx, %edx

	/* commit the new semaphore value */
	movl %edx, *%eax
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
	movl *%eax, %ecx

	/* get the current holding thread */
	movl %ecx, %edi
	shr  $16, %edi

	/* get the current generation */
	andl $0x0000FFFF, %ecx 

	/* is the last thread to try to take the lock, our thread? */
	cmpl %edi, %ebx
	je   no_contention

	/* if contention, update generation */
	inc  %ecx
	/* loop around with the addition */
	cmpl $0x0000FFFF, %ecx 
	ja   no_contention
	movl $0, %ecx
no_contention:
	/* commit the new semaphore value */
	movl %ecx, *%eax
.globl cos_atomic_user2_end
cos_atomic_user2_end:
	ret

/**
 * Copyright 2008 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

.data
/* This symbol exists so that this object is forced to load on link */
.globl cos_force_sched_link
cos_force_sched_link:	
.long 0
	
.text
/*
 * Can't modify the in regs as we might start over (rollback)
 * in:  %eax = lockaddr, %ebx = curr_thread_id
 * out:	%edi = lock value
 */
.globl cos_atomic_user1
.type cos_atomic_user1, @function
.align 16
cos_atomic_user1:
	movl (%eax), %edi
	/* is the lock taken? */
	cmpl $0, %edi
	jne  cos_atomic_user1_end /* edi will fall through as 0 */
	/* commit the new lock value: the current thread */
	movl %ebx, (%eax)
.globl cos_atomic_user1_end
cos_atomic_user1_end:
	ret
		
/*
 * cos_sched_lock_release:	
 *
 * in:  %eax = lockaddr
 * out: %ecx = blocked thd/curr_thd packed pair
 */
.globl cos_atomic_user2 
.type cos_atomic_user2, @function
.align 16
cos_atomic_user2:
	movl (%eax), %ecx
	movl $0, (%eax) 
.globl cos_atomic_user2_end
cos_atomic_user2_end:
	ret

.globl cos_atomic_user3
.type cos_atomic_user3, @function
cos_atomic_user3:
.globl cos_atomic_user3_end
cos_atomic_user3_end:
.globl cos_atomic_user4
.type cos_atomic_user4, @function
cos_atomic_user4:
.globl cos_atomic_user4_end
cos_atomic_user4_end:
	movl $0, %eax
	movl (%eax), %eax

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

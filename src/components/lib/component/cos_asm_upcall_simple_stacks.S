/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

//#define __ASM__
#include <consts.h>
#include <cos_asm_simple_stacks.h>
#include "../../../kernel/include/asm_ipc_defs.h"
#if defined(__x86__)
#include "arch/x86/cos_asm_upcall_simple_stacks.h"
#elif defined(__x86_64__)
#include "arch/x86_64/cos_asm_upcall_simple_stacks.h"
#elif defined(__arm__)
#include "arch/arm/cos_asm_upcall_simple_stacks.h"
#endif

#define IPRETURN 4

.section .initonce
.align 16
.globl nil
nil:
	.rep 1024
	.long 0
	.endr

COS_STATIC_STACK

COS_DEFAULT_STACK_ACQUIRE

COS_UPCALL_ENTRY

/*
 * %eax = cmpval, %ebx = memaddr, %ecx = newval
 * output %edx, either cmpval: fail, or newval:	success
 */
.weak cos_atomic_cmpxchg
.type cos_atomic_cmpxchg, %function
.align 16
cos_atomic_cmpxchg:
.weak cos_atomic_cmpxchg_end
cos_atomic_cmpxchg_end:
	COS_ATOMIC_CMPXCHG_END

/*
 * %eax = semaphore_addr, %ebx = thread_id, %ecx = count
 */
.weak cos_atomic_user1
.type cos_atomic_user1, %function
cos_atomic_user1:
.weak cos_atomic_user1_end
cos_atomic_user1_end:
.weak cos_atomic_user2
.type cos_atomic_user2, %function
cos_atomic_user2:
.weak cos_atomic_user2_end
cos_atomic_user2_end:
.weak cos_atomic_user3
.type cos_atomic_user3, %function
cos_atomic_user3:
.weak cos_atomic_user3_end
cos_atomic_user3_end:
.weak cos_atomic_user4
.type cos_atomic_user4, %function
cos_atomic_user4:
.weak cos_atomic_user4_end
cos_atomic_user4_end:
	/* crash out as something's wrong */
	COS_ATOMIC_USER4_END
	

#include <dietfeatures.h>

	.text
	.align 2
	
	.global __unified_syscall
	.global __unified_syscall4

__unified_syscall4:
	ldmia	sp!, {r4, r5, r6}
__unified_syscall:
	cmn	r0, #4096
	movcc	pc, lr			@ return value comes direct from kernel.

	rsb	r1, r0, #0
#ifdef WANT_THREAD_SAFE
	stmdb	sp!, {r1, lr}
	bl	__errno_location
	ldmia	sp!, {r1, lr}
#else
	ldr	r0, .L0
#endif
	str	r1, [r0]		@ store error code into 'errno'
	mvn	r0, #0			@ return value = -1

/* here we go and "reuse" the return for weak-void functions */
#include "dietuglyweaks.h"

	mov	pc, lr			@ return

#ifndef WANT_THREAD_SAFE
.L0:	.long 	errno
#endif

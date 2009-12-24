
#include "dietfeatures.h"
#include "syscalls.h"

	.text
	.align 2

	.global _start
	.weak 	exit
	.global _exit

_start:

#ifdef WANT_DYNAMIC
	mov	a4, a1			@ save dynamic ld.so _fini
#endif
	mov	fp, #0			@ clear the frame pointer
	ldr	a1, [sp], #4		@ argc
	mov	a2, sp			@ argv
	ldr	ip, .L3
	add	a3, a2, a1, lsl #2	@ &argv[argc]
	add	a3, a3, #4		@ envp
#ifdef __DYN_LIB
	ldr	sl, .L4
1:	add	sl, pc, sl
	str	a3, [sl, ip]		@ environ = envp
#else
	str	a3, [ip, #0]		@ environ = envp
#endif

#ifdef PROFILING
	stmdb	sp!, { r0 - r3 }
	ldr	r0, .L5
	ldr	r1, .L6
	bl	monitor
	ldmia	sp!, { r0 - r3 }
#endif

#ifdef WANT_DYNAMIC
	bl	_dyn_start
#else
#ifdef WANT_STACKGAP
	bl	stackgap
#else
	bl	main
#endif
#endif

@
@ The exit status from main() is already in r0.
@ We need to branch to 'exit' in case we have linked with 'atexit'.
@
	bl	exit

exit:
_exit:

#ifdef PROFILING
	mov	r4, r0			@ save a copy of exit status
	bl	_stop_monitor
	mov	r0, r4
#endif
	swi	$__NR_exit		@ never returns.


#ifdef __DYN_LIB
.L3:	.word 	environ(GOT)
.L4:	.word 	_GLOBAL_OFFSET_TABLE_-(1b+8)
#else
.L3:	.word 	environ
#endif

#ifdef PROFILING
.L5:	.word	.text
.L6:	.word	_etext
#endif




#include <setjmp.h>

.text
.weak setjmp
.type setjmp,@function
setjmp:
.globl __setjmp
.type __setjmp,@function
__setjmp:
	popl %eax
	popl %ecx
	pushl $0
	pushl %ecx
	pushl %eax

.globl __sigsetjmp
.type __sigsetjmp,@function
__sigsetjmp:
	movl 4(%esp), %eax

	/* Save registers.  */
	movl %ebx, (JB_BX*4)(%eax)
	movl %esi, (JB_SI*4)(%eax)
	movl %edi, (JB_DI*4)(%eax)
	movl %ebp, (JB_BP*4)(%eax)
	leal 4(%esp), %edx	/* Save SP as it will be after we return.  */
	movl 0(%esp), %ecx	/* Save PC we are returning to now.  */
	movl %edx, (JB_SP*4)(%eax)
	movl %ecx, (JB_PC*4)(%eax)

	/* Make a tail call to __sigjmp_save; it takes the same args.  */
#ifdef	PIC
	call	1f
	addl	$_GLOBAL_OFFSET_TABLE_, %ecx
	jmp	*__sigjmp_save@GOT(%ecx)
1:	movl	(%esp), %ecx
	ret
#else
	jmp __sigjmp_save
#endif
.size __sigsetjmp,.-__sigsetjmp;

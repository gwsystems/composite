#include "dietfeatures.h"
#include "PIC.h"

.text
.global _start
_start:
#if 1
	popl	%ecx			/* %ecx = argc */
	movl	%esp,%esi		/* %esi = argv */
	pushl	%ecx
#else
	movl	(%esp),%ecx		/* %ecx = argc */
	leal	4(%esp),%esi		/* %esi = argv */
#endif
	leal	4(%esi,%ecx,4),%eax	/* %eax = envp = (4*ecx)+%esi+4 */

#ifdef WANT_DYNAMIC
/* in %edx we have the ld.so dynamic _fini ( register this if non null ) */
	push	%edx
#endif

	pushl	%eax
	pushl	%esi
	pushl	%ecx
	PIC_INIT			/* non-PIC: this is an empty line */
	PUT_VAR %eax, environ, %ecx	/* non-PIC: movl %eax,environ */

#ifdef PROFILING
	pushl	$_etext
	pushl	$.text
	call	monitor
	addl	$0x8, %esp
#endif

#ifdef WANT_SYSENTER
	movl	%eax,%edx
	xorl	%esi,%esi
1:
	add	$4,%edx
	cmpl	%esi,-4(%edx)
	jne	1b
1:
	movl	(%edx),%edi
	test	%edi,%edi
	jz	1f
	addl	$8,%edx
	cmpl	$32,%edi
	jne	1b
	movl	-4(%edx),%edi
	PUT_VAR %edi, __vsyscall, %edx
1:
#endif

#ifdef WANT_DYNAMIC
	call	_dyn_start
#else
#ifdef WANT_STACKGAP
	call	stackgap
#else
	call	main
#endif
#endif

#ifdef PROFILING
	pushl	%eax
	call	_stop_monitor
	popl	%eax
#endif
	pushl	%eax
	call	exit
	hlt	/* die now ! will ya ... */
.Lstart:
	.size	 _start,.Lstart-_start


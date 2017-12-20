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

	pushl	%eax
	pushl	%esi
	pushl	%ecx

	call	main
	pushl	%eax
	call	exit
	hlt	/* die now ! will ya ... */
.Lstart:
	.size	 _start,.Lstart-_start

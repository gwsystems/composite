.section .rodata
sysenter_message:
	.string	"System calls aren't quite implemented yet. Sorry\n"

.text
.global sysenter_interposition_entry
sysenter_interposition_entry:
	pusha
	movl	$sysenter_message, %edi
	push	%edi
	push	$5
	call	printk
	sub	$8, %esp
	popa
	sysexit

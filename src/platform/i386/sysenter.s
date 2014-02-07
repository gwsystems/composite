.section .rodata
sysenter_message:
	.string	"Test message\n"

.text
.global sysenter
sysenter:
	pusha
	movl	$sysenter_message, %esi
	movl	$1, %edi
	movl	$0, %eax
	//call	printk
	popa
	sysexit

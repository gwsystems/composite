sysenter_message:
	.asciz "Entered system mode"

.global sysenter, _sysenter
sysenter:
_sysenter:
	push sysenter_message
	push $0
	call printk
	sysexit

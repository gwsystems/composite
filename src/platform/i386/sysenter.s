.global syscall
syscall:
	#mov %ecx,<stack pointer>
	#mov %edx,<return address>
	sysenter

.global sysenter
sysenter:
	#do stuff
	sysexit

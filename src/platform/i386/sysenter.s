.global sysenter
sysenter:
	pusha
	mov 0x3fd, %dx
	inb %dx
        mov 0x55, %al
        mov 0x3f8, %dx
        outb %al, %dx
	popa
	sysexit

.global test_user_function
test_user_function:
	sysenter
	mov 0x3fd, %dx
	inb %dx
	mov 0x55, %al
	mov 0x3f8, %dx
	outb %al, %dx
	jmp -4

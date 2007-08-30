.text
.align	2
.weak	waitpid
.type	waitpid, %function
waitpid:
.global	__libc_waitpid
.type	__libc_waitpid, %function
__libc_waitpid:
	mov	r3, #0
	b	wait4
	.size	waitpid, .-waitpid

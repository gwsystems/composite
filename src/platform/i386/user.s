.align 4096

.global user_test
user_test:
	movl $0xDEADBEEF, %eax
	sysenter

.global user_test_end
user_test_end:
	ret

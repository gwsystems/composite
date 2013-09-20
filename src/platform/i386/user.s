.global user_function
user_function:
	ret

.global user__init
user__init:
	mov $0, %edx
	mov $0x00dffd00, %eax		# Flat segment
	mov $0x174, %ecx
	wrmsr
	# mov $user_instruction, %rdx
	# mov $user_stack, %ecx
	# sysexit
	## need to jump to user level code here
	ret

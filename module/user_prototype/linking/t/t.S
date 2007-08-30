.data
.long 0
hello:
.string "hello world\n"

.text
.globl main
main:
	movl $3,%eax
	movl $2,%ebx
	movl $hello,%ecx
	movl $12,%edx
	int $0x80

	ret

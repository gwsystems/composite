.text
.global gettimeofday
.type gettimeofday,@function
gettimeofday:
	mov	$0xffffffffff600000,%rax
	callq	*%rax
	cmpq	$-128, %rax
	jbe	.Lnoerror
	negl	%eax
	pushq	%rax
	call	__errno_location
	popq	%rcx
	movl	%ecx,(%rax)
	orq	$-1, %rax
.Lnoerror:
	ret
.Lhere:
	.size	 gettimeofday,.Lhere-gettimeofday

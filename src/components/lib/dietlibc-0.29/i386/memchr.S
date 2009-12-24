.text
.align 0
.global memchr
.type	 memchr,@function
memchr:
	pushl %edi
	movl 8(%esp),%edi
	movl 12(%esp),%eax
	movl 16(%esp),%ecx
	cld
	jecxz .Lnotfound

	repne scasb

	je .Lfound
.Lnotfound:
	xorl %edi, %edi
	incl %edi
.Lfound:
	movl %edi, %eax
	decl %eax

	popl %edi
	ret

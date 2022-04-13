#define COS_AINV_ENTRY		\
	push %rbx;		\
	push %rsi;		\
	push %rdi;		\
	push %rdx;		\
	push %rbp;		\
	movl %esp, %ebx;	\
	addl $24, %ebx;		\
	push %rbx;		\
	push %rax;		\
	call cos_async_inv;	\
	addl $8, %esp;		\
	cmpl $0, %eax;		\
	je ainv_ret;		\
	movl 24(%esp), %ebx;	\
	movl 28(%esp), %esi;	\
	movl 32(%esp), %edi;	\
	movl 36(%esp), %edx;	\
	movl %esp, %ebp;	\
	movabs $ainv_ret, %rcx;	\
	sysenter;

#define COS_AINV_RET		\
	pop %rbp;		\
	pop %rdx;		\
	pop %rdi;		\
	pop %rsi;		\
	pop %rbx;		\
	ret;

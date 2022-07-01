#define COS_AINV_ENTRY		\
	pushl %ebx;		\
	pushl %esi;		\
	pushl %edi;		\
	pushl %edx;		\
	pushl %ebp;		\
	movl %esp, %ebx;	\
	addl $24, %ebx;		\
	push %ebx;		\
	push %eax;		\
	call cos_async_inv;	\
	addl $8, %esp;		\
	cmpl $0, %eax;		\
	je ainv_ret;		\
	movl 24(%esp), %ebx;	\
	movl 28(%esp), %esi;	\
	movl 32(%esp), %edi;	\
	movl 36(%esp), %edx;	\
	movl %esp, %ebp;	\
	movl $ainv_ret, %ecx;	\
	sysenter;

#define COS_AINV_RET		\
	popl %ebp;		\
	popl %edx;		\
	popl %edi;		\
	popl %esi;		\
	popl %ebx;		\
	ret;

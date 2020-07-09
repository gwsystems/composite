
#if defined(__x86__)
#warning "working"
#define INV_TEST_SERVERFN			\
		movl %ebp, %esp;		\
		xor %ebp, %ebp;			\
                pushl %edi;			\
                pushl %esi;			\
                pushl %ebx;			\
                call test_serverfn;		\
                addl $12, %esp;			\
                movl %eax, %ecx;		\
                movl $RET_CAP, %eax;		\
                sysenter;
#else
#define INV_TEST_SSERVERFN		\
		nop;
#endif

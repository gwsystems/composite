
#if defined(__x86__)
#define INV_TEST_SERVERFN			\
.text;						\
.globl __inv_test_serverfn;			\
.type __inv_test_serverfn, @function;		\
__inv_test_serverfn:				\
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
#define INV_TEST_SERVERFN
#endif

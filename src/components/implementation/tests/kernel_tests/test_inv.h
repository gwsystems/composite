
#if defined(__x86__)
#define INV_TEST_SERVERFN                      \
	.text;                                 \
	.globl __inv_test_serverfn;            \
	.type  __inv_test_serverfn, @function; \
	__inv_test_serverfn:                   \
	movl % ebp, % esp;                     \
	xor % ebp, % ebp;                      \
	pushl % edi;                           \
	pushl % esi;                           \
	pushl % ebx;                           \
	call test_serverfn;                    \
	addl $12, % esp;                       \
	movl % eax, % ecx;                     \
	movl $RET_CAP, % eax;                  \
	sysenter;

#elif defined(__arm__)
#define INV_TEST_SERVERFN                       \
	.text;                                  \
	.globl __inv_test_serverfn;             \
	.type  __inv_test_serverfn, % function; \
	__inv_test_serverfn:                    \
	mov r0, r2;                             \
	mov r1, r3;                             \
	mov r2, r4;                             \
	ldr r6, = test_serverfn;                \
	blx r6;                                 \
	ldr r1, = RET_CAP;                      \
	svc #0x00;                              \
	.ltorg;

#endif

/* Use the passed in stack in arg4 */

#define RET_CAP (1 << 16)

.text
.globl __inv_test_serverfn
.type  __inv_test_serverfn, @function
__inv_test_serverfn:
	movl %ebp, %esp
	xor %ebp, %ebp
        pushl %edi
        pushl %esi
        pushl %ebx
        call test_serverfn
        addl $12, %esp
        movl %eax, %ecx
        movl $RET_CAP, %eax
        sysenter;

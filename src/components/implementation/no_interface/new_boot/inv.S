/* Use the passed in stack in arg4 */

#include <cos_asm_simple_stacks.h>

#define RET_CAP (1 << 16)
.text
.globl __inv_test_entry
.type  __inv_test_entry, @function
__inv_test_entry:
	COS_ASM_GET_STACK

	call boot_thd_done

	COS_ASM_RET_STACK
	movl %eax, %ecx
	movl $RET_CAP, %eax
	sysenter;

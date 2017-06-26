/* Use the passed in stack in arg4 */

#define __ASM__
#include <consts.h>
#include <cos_asm_simple_stacks.h>

.text
.globl __inv_test_entry
.type  __inv_test_entry, @function
__inv_test_entry:
	COS_ASM_GET_STACK

	pushl %ebp /* arg 4 */
        pushl %edi /* arg 3 */
        pushl %esi /* arg 2 */
        pushl %ebx /* arg 1 */
	call boot_thd_done
        movl %eax, %ecx

	COS_ASM_RET_STACK
	movl $RET_CAP, %eax
	sysenter;

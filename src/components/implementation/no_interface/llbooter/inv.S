/* Use the passed in stack in arg4 */

#define __ASM__
#include <consts.h>
#include <cos_asm_simple_stacks.h>

.text
.globl __inv_llboot_entry
.type  __inv_llboot_entry, @function
__inv_llboot_entry:
	COS_ASM_GET_STACK

	pushl $0		/* ret 3 */
	movl  %esp, -8(%esp)	/* ret 3 ptr */
	pushl $0		/* ret 2 */
	movl  %esp, -8(%esp)	/* ret 2 ptr */
	subl  $8, %esp		

	pushl %ebp		/* arg 4 */
        pushl %edi		/* arg 3 */
        pushl %esi		/* arg 2 */
        pushl %ebx		/* arg 1 */
	call llboot_entry
	addl $24, %esp
	popl %esi		/* ret 2 */
	popl %edi		/* ret 3 */

        movl %eax, %ecx

	COS_ASM_RET_STACK
	movl $RET_CAP, %eax
	sysenter;

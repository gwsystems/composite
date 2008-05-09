#include <cos_asm_stacks.h>

#ifdef NIL	
#define RET_CAP ((1<<20)-1)
#define IPRETURN 4

.text	
/* delivered to the server service all calls are demuxed through here
   so that we can have a uniform infrastructure for returning (through 
   ipc_server_ret_marshal) */

.globl mman_get_page_inv
.type  mman_get_page_inv, @function
.align 16
mman_get_page_inv:
	COS_ASM_GET_STACK

	pushl %edi 
	pushl %esi
/*	pushl %ebx*/
	pushl %ecx
	call mman_get_page
	/* addl $16, %esp <- no need to clean up */
	
	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */

	sysenter

.globl mman_release_page_inv
.type  mman_release_page_inv, @function
.align 16
mman_release_page_inv:
	COS_ASM_GET_STACK

	pushl %edi 
	pushl %esi
/*	pushl %ebx */
	pushl %ecx
	call mman_release_page
	/* addl $16, %esp <- no need to clean up */
	
	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */

	sysenter
#endif

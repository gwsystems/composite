/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <cos_asm_stacks.h>
	
#define RET_CAP ((1<<20)-1)
#define IPRETURN 4

.text	
/* delivered to the server service all calls are demuxed through here
   so that we can have a uniform infrastructure for returning (through 
   ipc_server_ret_marshal) */
	
.globl sched_init_inv
.type  sched_init_inv, @function
.align 16
sched_init_inv:
	COS_ASM_GET_STACK
	
/*	movl %ebp, %esp */
/*	popl %ebp*/
	call sched_init
/*	pushl %ebp*/

	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */

	sysenter

.globl sched_report_processing_inv
.type  sched_report_processing_inv, @function
.align 16
sched_report_processing_inv:
	COS_ASM_GET_STACK
	
	pushl %ebp
	pushl %edi
	pushl %esi
	pushl %ebx /* length */
	call sched_report_processing

	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */

	sysenter

.globl sched_create_child_brand_inv
.type  sched_create_child_brand_inv, @function
.align 16
sched_create_child_brand_inv:
	COS_ASM_GET_STACK
	
	pushl %ebp
	pushl %edi
	pushl %esi
	pushl %ebx /* length */
	call sched_create_child_brand

	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */

	sysenter

.globl sched_child_yield_thd_inv
.type  sched_child_yield_thd_inv, @function
.align 16
sched_child_yield_thd_inv:
	COS_ASM_GET_STACK
	
	pushl %ebp
	pushl %edi
	pushl %esi
	pushl %ebx /* length */
	call sched_child_yield_thd

	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */

	sysenter

.globl sched_wakeup_inv
.type  sched_wakeup_inv, @function
.align 16
sched_wakeup_inv:
	COS_ASM_GET_STACK
	
	pushl %ebp
	pushl %edi
	pushl %esi
	pushl %ebx /* length */
	call sched_wakeup

	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */

	sysenter

.globl sched_block_inv
.type  sched_block_inv, @function
.align 16
sched_block_inv:
	COS_ASM_GET_STACK
	
	pushl %ebp
	pushl %edi
	pushl %esi
	pushl %ebx /* length */
	call sched_block

	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */

	sysenter

.globl sched_create_net_upcall_inv
.type  sched_create_net_upcall_inv, @function
.align 16
sched_create_net_upcall_inv:
	COS_ASM_GET_STACK
	
	pushl %ebp
	pushl %edi
	pushl %esi
	pushl %ebx /* length */
	call sched_create_net_upcall

	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */

	sysenter



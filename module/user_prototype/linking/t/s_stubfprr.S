/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <cos_asm_server_stub.h>

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

.globl sched_suspend_thd_inv
.type  sched_suspend_thd_inv, @function
.align 16
sched_suspend_thd_inv:
	COS_ASM_GET_STACK
	
	pushl %ebp
	pushl %edi
	pushl %esi
	pushl %ebx /* length */
	call sched_suspend_thd

	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */

	sysenter

.globl sched_resume_thd_inv
.type  sched_resume_thd_inv, @function
.align 16
sched_resume_thd_inv:
	COS_ASM_GET_STACK
	
	pushl %ebp
	pushl %edi
	pushl %esi
	pushl %ebx /* length */
	call sched_resume_thd

	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */

	sysenter

cos_asm_server_stub_spdid(sched_yield)

.globl sched_wakeup_inv
.type  sched_wakeup_inv, @function
.align 16
sched_wakeup_inv:
	COS_ASM_GET_STACK
	
	pushl %ebp
	pushl %edi
	pushl %esi
//	pushl %ebx /* length */
	pushl %ecx /* spdid */
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
	pushl %ecx /* spdid */
	call sched_block

	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */

	sysenter

cos_asm_server_stub_spdid(sched_timeout)
cos_asm_server_stub_spdid(sched_timeout_thd)

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

.globl sched_exit_inv
.type  sched_exit_inv, @function
.align 16
sched_exit_inv:
	COS_ASM_GET_STACK
	
/*	movl %ebp, %esp */
/*	popl %ebp*/
	call sched_exit
/*	pushl %ebp*/

	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */

	sysenter

cos_asm_server_stub(sched_create_thread)
cos_asm_server_stub(sched_tick_freq)
cos_asm_server_stub(sched_timestamp)
	
.globl sched_component_take_inv
.type  sched_component_take_inv, @function
.align 16
sched_component_take_inv:
	COS_ASM_GET_STACK
	
	pushl %ebp
	pushl %edi
	pushl %esi
	pushl %ebx /* length */
	pushl %ecx /* spdid */
	call sched_component_take

	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */

	sysenter

	
.globl sched_component_release_inv
.type  sched_component_release_inv, @function
.align 16
sched_component_release_inv:
	COS_ASM_GET_STACK
	
	pushl %ebp
	pushl %edi
	pushl %esi
	pushl %ebx
	pushl %ecx /* spdid */
	call sched_component_release

	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */

	sysenter

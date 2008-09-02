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

.globl lock_component_pretake_inv
.type  lock_component_pretake_inv, @function
.align 16
lock_component_pretake_inv:
	COS_ASM_GET_STACK
	
	pushl %ebp
	pushl %edi
	pushl %esi
//	pushl %ebx /* length */
	pushl %ecx /* spdid */
	call lock_component_pretake

	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */

	sysenter

.globl lock_component_take_inv
.type  lock_component_take_inv, @function
.align 16
lock_component_take_inv:
	COS_ASM_GET_STACK
	
	pushl %ebp
	pushl %edi
	pushl %esi
//	pushl %ebx /* length */
	pushl %ecx /* spdid */
	call lock_component_take

	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */

	sysenter

.globl lock_component_release_inv
.type  lock_component_release_inv, @function
.align 16
lock_component_release_inv:
	COS_ASM_GET_STACK
	
	pushl %ebp
	pushl %edi
	pushl %esi
//	pushl %ebx /* length */
	pushl %ecx /* spdid */
	call lock_component_release

	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */

	sysenter

.globl lock_component_alloc_inv
.type  lock_component_alloc_inv, @function
.align 16
lock_component_alloc_inv:
	COS_ASM_GET_STACK
	
	pushl %ebp
	pushl %edi
	pushl %esi
	pushl %ebx /* length */
	pushl %ecx /* spdid */
	call lock_component_alloc

	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */

	sysenter

.globl lock_component_free_inv
.type  lock_component_free_inv, @function
.align 16
lock_component_free_inv:
	COS_ASM_GET_STACK
	
	pushl %ebp
	pushl %edi
	pushl %esi
//	pushl %ebx /* length */
	pushl %ecx /* spdid */
	call lock_component_free

	movl %eax, %ecx
	movl $RET_CAP, %eax /* return capability */

	sysenter

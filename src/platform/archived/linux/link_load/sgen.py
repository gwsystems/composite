#!/usr/bin/python

import re
import sys
import string

header = """
#include <consts.h>
#include <asm_ipc_defs.h>
#define PROTOTYPE		
.text
"""

cSS = """
.globl cos_stub_c_%(fn_name)s
.align 16
cos_stub_c_%(fn_name)s:
	movl CAPNUM(%eax), %eax /* typeof(%eax) = (usr_inv_cap*) */
	pushl %ebp

	movl IPRETURN(%esp), %edx /* user-defined value */
	movl %esp, %ebp /* save the stack */
	movl $cos_stub_cr_%(fn_name)s, %ecx /* save the return ip */

#ifdef PROTOTYPE
	movl kern_stack, %esp
	jmp kernel_ipc_syscall
#else
	sysenter
#endif

/* stub for return */
.globl cos_stub_cr_%(fn_name)s
.align 4
cos_stub_cr_%(fn_name)s:
	/* replace the correct return address */
	movl %ebp, IPRETURN(%esp)
	popl %ebp
	ret
"""

sSS = """
/* delivered to the server service all calls are demuxed through here
   so that we can have a uniform infrastructure for returning */
.globl cos_stub_s_%(fn_name)s
.align 16
cos_stub_s_%(fn_name)s:
	movl %ebp, %esp
	/* pops here to support stack sharing */
	popl %ebp
	addl $4, %esp
	call *%eax
	pushl $0 /* space for the retaddr */
	pushl %ebp
	movl %eax, %ecx
	movl $0, %eax /* return capability */
#ifdef PROTOTYPE
	movl kern_stack, %esp
	jmp kernel_ipc_syscall
#else
	sysenter
#endif
"""

if (len(sys.argv) > 4):
    print "Usage: "+sys.argv[0]+" <fn_name> <arg signature> <c|s (client or server)>"
    sys.exit(1)


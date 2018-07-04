#ifndef COS_ASM_CLIENT_STUB_H
#define COS_ASM_CLIENT_STUB_H

/*
 * This file contains the client-side assembly stubs for a synchronous
 * invocation (sinv).  It should be included in the assembly files
 * that constitute the stubs in the src/component/interface/.../stubs/
 * directories.  This part of the synchronous invocation path is very
 * much like the Procedure Linkage Table (PLT) and indirects through a
 * trampoline stub that parses a data-structure that locates the
 * actual stub.
 *
 * This code creates
 *
 * 1. the user-level capability (ucap) structure that contains the
 *    address of the stub to invoke when one of the server's interface
 *    functions is invoked, and
 *
 * 2. the trampoline that actually implements the function's symbol,
 *    and redirects the call to the stub, passing a reference to the
 *    ucap to it.  That ucap contains the capability that should be
 *    used for the kernel sinv.
 */

#define __ASM__
#include <consts.h>

#define cos_asm_client_stub(name)              \
.text;                                         \
.globl name;                                   \
.type  name, @function;			       \
.align 8 ;                                     \
name:                                          \
        movl $__cosrt_ucap_##name, %eax ;      \
        jmp *INVFN(%eax) ;                     \
                                               \
.section .ucap, "a", @progbits ;               \
.globl __cosrt_ucap_##name ;                   \
__cosrt_ucap_##name:                           \
        .rep UCAP_SZ ;                         \
        .long 0 ;                              \
        .endr ;				       \
.text

/* start out in the text segment, and always return there */
.text

#endif /* COS_ASM_CLIENT_STUB_H */

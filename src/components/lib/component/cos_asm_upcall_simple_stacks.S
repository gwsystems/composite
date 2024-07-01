//#define __ASM__
;; #include <consts.h>
;; #include <cos_asm_simple_stacks.h>
;; #include "../../../kernel/include/shared/asm_ipc_defs.h"
;; #if defined(__x86__)
;; #include "arch/x86/cos_asm_upcall_simple_stacks.h"
;; #elif defined(__x86_64__)
;; #include "arch/x86_64/cos_asm_upcall_simple_stacks.h"
;; #elif defined(__arm__)
;; #include "arch/arm/cos_asm_upcall_simple_stacks.h"
;; #endif

#include <cos_regs.h>

;; #define IPRETURN 4

;; .section .initonce
;; .align 16
;; .globl nil
;; nil:
;; 	.rep 1024
;; 	.long 0
;; 	.endr

;; COS_STATIC_STACK

;; COS_UPCALL_ENTRY

REGS_USER_UPCALL(__cosrt_upcall_entry, cos_upcall_fn)

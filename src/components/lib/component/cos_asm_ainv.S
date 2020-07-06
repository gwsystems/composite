#if defined(__x86__)
#include "arch/x86/cos_asm_ainv.h"
#elif defined(__arm__)
#include "arch/arm/cos_asm_ainv.h"
#endif

/* Async inv stub. */
	
.globl cos_ainv_entry
.type  cos_ainv_entry, %function
.align 16
cos_ainv_entry:
	COS_AINV_ENTRY

ainv_ret:
	COS_AINV_RET


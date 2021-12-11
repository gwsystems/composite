#ifndef COS_ASM_STUB_H
#define COS_ASM_STUB_H

#if defined(__x86__)
#include "arch/x86/cos_asm_stubs.h"
#elif defined(__x86_64__)
#include "arch/x86_64/cos_asm_stubs.h"
#elif defined(__arm__)
#include "arch/arm/cos_asm_stubs.h"
#else
#error "Undefined architecture!"
#endif

#endif	/* COS_ASM_STUB_H */

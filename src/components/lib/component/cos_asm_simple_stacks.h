#ifndef COS_ASM_SIMPLE_STACKS_H
#define COS_ASM_SIMPLE_STACKS_H

#include <consts.h>

#ifndef MAX_STACK_SZ_BYTE_ORDER
#error "Missing MAX_STACK_SZ_BYTE_ORDER, try including consts.h"
#endif

#if defined(__x86__)
#include "arch/x86/cos_asm_simple_stacks.h"
#elif defined(__arm__)
#include "arch/arm/cos_asm_simple_stacks.h"
#else
#error "Undefined architecture!"
#endif

#endif

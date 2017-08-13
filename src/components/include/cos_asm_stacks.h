#ifndef COS_ASM_STACKS_H
#define COS_ASM_STACKS_H

#include <cos_stkmgr_configure.h>
#if ENABLE_STACK_MANAGER==1
#include <cos_asm_stkmgr_stacks.h>
#else
#include <cos_asm_simple_stacks.h>
#endif

#endif

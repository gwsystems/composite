#pragma once

#include <cos_compiler.h>

#ifndef COS_MAX_NUM_THREADS
#define COS_MAX_NUM_THREADS 64
#endif

#ifndef COS_MAX_NUM_CORES
#define COS_MAX_NUM_CORES 1
#endif

#ifndef COS_STACK_SIZE_ORDER
#define COS_STACK_SIZE_ORDER 14
#endif

#define COS_STACK_SIZE (1 << COS_STACK_SIZE_ORDER)

/*
 * Stack acquisition methods on upcall into a component. Only one of
 * these should be `1`, and the rest should be `0`.
 *
 * ## `COS_STATIC_DIRECT_MAPPED_STACKS`
 *
 * This policy simply allocates the stacks as a linear array, and
 * directly locates the stack to use by indexing by thread id. This is
 * likely the fastest option, but can waste a lot of memory (i.e. a
 * stack is allocated for a thread in every component, even when that
 * thread won't execute in each).
 *
 * ## `COS_STATIC_SINGLE_STACK`
 *
 * The component only has a single stack as it should execute only one
 * thread.
 *
 * ## `COS_STATIC_PERCORE_SINGLE_STACK`
 *
 * The component only has a single stack *per-core* as it should
 * execute at max only one thread on each core.
 */
#ifndef COS_STATIC_DIRECT_MAPPED_STACKS
#define COS_STATIC_DIRECT_MAPPED_STACKS 1
#endif

#ifndef COS_STATIC_SINGLE_STACK
#define COS_STATIC_SINGLE_STACK 0
#endif

#ifndef COS_STATIC_PERCORE_SINGLE_STACK
#define COS_STATIC_PERCORE_SINGLE_STACK 0
#endif

#ifndef __ASSEMBLER__
COS_STATIC_ASSERT(((COS_STATIC_DIRECT_MAPPED_STACKS + COS_STATIC_SINGLE_STACK + COS_STATIC_PERCORE_SINGLE_STACK) == 1),
		  "Only a single stack allocation mechanism should be active for each component.");
#endif

#pragma once

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
 */
#ifndef COS_STATIC_DIRECT_MAPPED_STACKS
#define COS_STATIC_DIRECT_MAPPED_STACKS 1
#endif

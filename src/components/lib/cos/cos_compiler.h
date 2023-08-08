#pragma once

#ifndef NULL
#define NULL ((void *)0)
#endif
#ifndef unlikely
#define unlikely(p) __builtin_expect((p), 0)
#endif
#ifndef likely
#define likely(p)   __builtin_expect((p), 1)
#endif

#define COS_PAGE_ALIGNED  __attribute__((aligned(COS_PAGE_SIZE)))
#define COS_CACHE_ALIGNED __attribute__((aligned(COS_COHERENCE_UNIT_SIZE)))
/* Functions marked as fastpaths should generate code without function calls */
#define COS_FASTPATH      __attribute__((always_inline,flatten))
/* Simple forced inlining. Can replace the *inline* keyword */
#define COS_FORCE_INLINE  __attribute__((always_inline))
/* Prevent inlining. Forces error paths to be in separate functions */
#define COS_NEVER_INLINE  __attribute__((noinline))
/* This function will never return */
#define COS_NO_RETURN     __attribute__((noreturn))
#define COS_WEAK          __attribute__((weak))

#ifndef COS_STATIC_ASSERT
#ifndef __cplusplus
#define COS_STATIC_ASSERT(cond, err_msg) _Static_assert(cond, err_msg)
#else
/* g++ doesnt allow _Static_assert; compiling the c++ code will throw an error */
#define COS_STATIC_ASSERT(cond, err_msg) static_assert(cond, err_msg)
#endif
#endif

#ifndef offsetof
#define offsetof(type, member) __builtin_offsetof(type, member)
#endif

#ifndef STRINGIFY
#define STRINGIFY(...) #__VA_ARGS__
#define EXPAND(X) STRINGIFY(X)
#endif

#ifndef COS_STUBS_H
#define COS_STUBS_H

/*
 * The macros that help write the C stub functions for synchronous
 * invocation (IPC) functions with non-default calling conventions.
 *
 * Default is 4 registers passed, one returned.
 *
 * The naming conventions encoded here are interpreted by the system
 * specification, thus they are the equivalent of a very simplified
 * C++ name mangling.
 */

#include <cos_kernel_api.h>
#include <cos_types.h>

/*
 * Macros for defining C stubs for the client and server. These
 * functions are called through the PLT-like level of indirection on
 * interface functions that ensure the the invocation information
 * (struct usr_inv_cap) which includes the capability id.
 */
#define COS_CLIENT_STUB(_type, _client_fn) __attribute__((regparm(1))) _type __cosrt_c_##_client_fn

/*
 * Note that this provides the complete prototype as it is fixed for
 * the non-fast-path version.
 */
#define COS_SERVER_STUB(_type, _server_fn)				\
_type									\
__cosrt_s_cstub_##_server_fn##(word_t p0, word_t p1, word_t p2, word_t p3, word_t *r1, word_t *r2)

#define COS_SERVER_3RET_STUB(_type, _server_fn)			\
_type								\
__cosrt_s_cstub_##_server_fn(word_t p0, word_t p1, word_t p2, word_t p3, word_t *r1, word_t *r2)

/*
 * Macros for library-based definitions of interface functions.
 *
 * These functions are used to allow a component that *defines* and
 * *exports* an interface (a set of functions) to also *depend* on
 * that same interface, and call the component it depends on, even
 * though the depended on function symbols are identical to those
 * provided (i.e. that are exported).
 *
 * An example of its use follows. In the interface stubs defined
 * within the client C files (c_*.c files):
 *
 * ```
 * int
 * COS_STUB_LIBFN(foo)(int arg)
 * {
 *   ...
 * }
 * COS_STUB_ALIAS(foo);
 * ```
 *
 * ...and in the interface's .h file:
 *
 * ```
 * int foo(int arg);
 * int COS_STUB_DECL(foo)(int arg);
 * ```
 *
 * ...and in any component/library that wishes to invoke the function
 * it depends on, rather than the one it perhaps provides:
 *
 * ```
 * int x = COS_EXTERN_INV(foo)(0);
 * ```
 *
 * To motiviate this functionality, a concrete example using
 * `foo`. Imagine that `foo` wishes to invoke `foo` in the depended-on
 * `foo`.
 *
 * ```
 * int
 * COS_STUB_LIBFN(foo)(int arg)
 * {
 *   return COS_EXTERN_INV(foo)(arg);
 * }
 * ```
 *
 * In normal C code, this would result in a recursive call. The
 * intention here is to allow this component to invoke another,
 * despite the shared function names. See the `init` interface if
 * you'd like to see a real example of their use.
 */
#define COS_STUB_FN(fn) __cosrt_extern_##fn

/*
 * library functions in interfaces must define the function using the
 * following two macros.
 */
#define COS_STUB_LIBFN(fn) __cosrt_extern_##fn
#define COS_STUB_ALIAS(fn) COS_FN_WEAKALIAS(fn, COS_STUB_FN(fn))

/*
 * In the header file, this should accompany each prototype: another
 * identical prototype, but using this (unfortunately, __typeof__ does
 * not work statically to avoid the repeating of the function type
 * info).
 */
#define COS_STUB_DECL(fn) COS_STUB_FN(fn)

/* Calling an interface function  */
#define COS_EXTERN_INV(fn) COS_STUB_FN(fn)

#endif	/* COS_STUBS_H */

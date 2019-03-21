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

#define COS_CLIENT_STUB(_type, _client_fn) __attribute__((regparm(1))) _type __cosrt_c_##_client_fn

/*
 * Note that this provides the complete prototype as it is fixed for
 * the non-fast-path version.
 */
#define COS_SERVER_STUB(_type, _server_fn) \
__attribute__((regparm(1))) _type	   \
__cosrt_s_cstub_##_server_fn##(struct usr_inv_cap *uc, word_t p0, word_t p1, word_t p2, word_t p3, word_t *r1, word_t *r2)

#define COS_SERVER_3RET_STUB(_type, _server_fn) \
__attribute__((regparm(1))) _type	   \
__cosrt_s_cstub_##_server_fn(struct usr_inv_cap *uc, word_t p0, word_t p1, word_t p2, word_t p3, word_t *r1, word_t *r2)

#endif	/* COS_STUBS_H */

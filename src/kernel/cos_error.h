#pragma once

/***
 * `cos_error.h` provides the error handling codes for all kernel
 * paths, and should be extended by the core system to also include
 * errors from core component functions.
 *
 * This file does *not* include facilities for printing out the error
 * code in a human-readable form. This should be defined in a .c file
 * to avoid replication of the memory-hungry print statements. TODO:
 * provide the error printing function: `cos_retval_description`, and
 * a function to exit with a configurable, and retval-defined error
 * upon error (similar to rust's `unwrap`).
 */

#include <cos_types.h>
#include <compiler.h>

/* Nominal and error return values */
#define COS_RET_SUCCESS                  0  /* Successful operation! */
#define COS_ERR_OUT_OF_BOUNDS            1  /* Index is used that is out of bounds */
#define COS_ERR_WRONG_PAGE_TYPE          2  /* Trying to use page as a type, when it is another */
#define COS_ERR_WRONG_CAP_TYPE           3  /* Trying to use capability as one type, when it is another */
#define COS_ERR_NOT_QUIESCED             4  /* Resource cannot be manipulated as it needs more time to quiesce */
#define COS_ERR_ALREADY_EXISTS           5  /* Cannot use slot as it is already used */
#define COS_ERR_STILL_REFERENCED         6  /* Cannot retype as the page is still referenced */
#define COS_ERR_MAINTAINS_REFERENCES     7  /* Resource still has references to other resources that must be removed */
#define COS_ERR_EPOCH_LIMIT              8  /* Has the epoch count reached its maximum value? */
#define COS_ERR_WRONG_INPUT_TYPE         9  /* A type input into a function is out of bounds */
#define COS_ERR_NOT_LIVE                 10 /* The referenced resource is not live */
#define COS_ERR_NO_MATCH                 11 /* The resource is not a match in the namespace */
#define COS_ERR_RESOURCE_NOT_FOUND       12 /* The resource cannot be found at an address */
#define COS_ERR_INSUFFICIENT_PERMISSIONS 13 /* Attempted operations without corresponding permission */
#define COS_ERR_CAP_LOOKUP               14 /* A capability lookup error: not present or incorrect type */
#define COS_ERR_CONTENTION               15 /* Another core has contended this operation, and won the race */
#define COS_ERR_NO_OPERATION             16 /* Another core has contended this operation, and won the race */

/*
 * General return value from the kernel. Negative values indicate
 * COS_ERR_* values. Other values are either COS_RET_SUCCESS, or a
 * non-negative return value.
 */
typedef word_t         cos_retval_t;

/*
 * `COS_WRAP`: Wrap an offset into a fixed-size, power of two-sized
 * value. Returns the wrapped value. We use this instead of bounds
 * checks to avoid the condition, and maintain kernel safety on array
 * accesses.
 */
#define COS_WRAP(off, bound) ((off) & ((bound) - 1))

/*
 * `COS_CHECK`: If the function does not return COS_RET_SUCCESS, then
 * return its error value.
 */
#define COS_CHECK(fn) do {						\
		cos_retval_t ___chk_ret = fn;				\
		if (unlikely(___chk_ret != COS_RET_SUCCESS)) return ___chk_ret; \
	} while (0)

/*
 * `COS_THROW`: place a return value into a pre-existing return
 * variable, and jump to the label. This implements C's local
 * exception handling.
 */
#define COS_THROW(retvar, retval, label) do {	\
		retvar = retval;		\
		goto label;			\
	} while (0)

/*
 * `COS_CHECK_THROW`: Combine check and throw above by gotoing to a
 * label if the function returns non-COS_RET_SUCCESS (setting a return
 * variable to the erroneous value).
 */
#define COS_CHECK_THROW(fn, retvar, label) do {				\
		cos_retval_t ___chk_ret = fn;				\
		if (unlikely(___chk_ret != COS_RET_SUCCESS)) COS_THROW(retvar, ___chk_ret, label); \
	} while (0)

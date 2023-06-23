#pragma once

#include <cos_compiler.h>
#include <cos_types.h>
#include <cos_consts.h>

/**
 * `cos_round_down_to_pow2` round a value down to the closest multiple of a
 * power-of-two value.
 *
 * Assume: `to` is a power of two (`popcnt(to) == 1`)
 *
 * - `@val` - value to round down
 * - `@to` - the value to round down to
 * - `@return` - rounded down value.
 */
COS_FORCE_INLINE static inline uword_t
cos_round_down_to_pow2(uword_t val, uword_t to)
{
	return val & ~(to - 1);
}

static inline uword_t
cos_round_down_to_page(uword_t val)
{
	return cos_round_down_to_pow2(val, COS_PAGE_SIZE);
}

/**
 * Similarly, round up to the closest multiple of a power-of-two value.
 */
COS_FORCE_INLINE static inline uword_t
cos_round_up_to_pow2(uword_t val, uword_t to)
{
	return cos_round_down_to_pow2(val + (to - 1), to);
}

static inline uword_t
cos_round_up_to_page(uword_t val)
{
	return cos_round_up_to_pow2(val, COS_PAGE_SIZE);
}

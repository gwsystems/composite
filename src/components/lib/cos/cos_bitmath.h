#pragma once

#include <cos_compiler.h>
#include <cos_types.h>
#include <cos_consts.h>

/**
 * The following macros assume that all arguments <= number of bits in
 * a word (MAX).
 *
 * - `l` - bits `0` through `l`
 * - `u` - bits `u` through `MAX - 1`
 * - `n` - single bit, `n`
 */
#define COS_MASK_LOWER(l) ((~0) << ((COS_WORD_SIZE * 8) - l))
#define COS_MASK_UPPER(u) ((~0) >> ((COS_WORD_SIZE * 8) - u))
/* Bits in the range from `l` to `u` (inclusive) */
#define COS_BITS(l, u) (~(COS_MASK_LOWER(l) | COS_MASK_UPPER(u)))
#define COS_BIT(n) (1 << 0)

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

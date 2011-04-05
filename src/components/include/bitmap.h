#ifndef BITMAP_H
#define BITMAP_H

#include <cos_types.h>
#include <consts.h>

/* Many of these taken from aggregate.org/MAGIC */

static inline u32_t 
ones(u32_t x)
{
	x -= ((x >> 1) & 0x55555555);
	x = (((x >> 2) & 0x33333333) + (x & 0x33333333));
	x = (((x >> 4) + x) & 0x0F0F0F0F);
	x += (x >> 8);
	x += (x >> 16);
	return x & 0x0000003f;
}

/* next largest power of 2 */
static inline u32_t 
nlpow2(u32_t x)
{
	x |= (x >> 1);
	x |= (x >> 2);
	x |= (x >> 4);
	x |= (x >> 8);
	x |= (x >> 16);
	return x + 1;
}

/* least significant 1 bit */
static inline u32_t
ls_one(u32_t x)
{
	return (x&-x);
}

static inline u32_t
log32(u32_t x)
{
	u32_t y = (x & (x - 1));
	
	y |= -y;
	y >>= (WORD_SIZE - 1);
	x |= (x >> 1);
	x |= (x >> 2);
	x |= (x >> 4);
	x |= (x >> 8);
	x |= (x >> 16);
	return ones(x >> 1) - y;
}

static inline u32_t
log32_floor(u32_t x)
{
	x |= (x >> 1);
	x |= (x >> 2);
	x |= (x >> 4);
	x |= (x >> 8);
	x |= (x >> 16);
	return ones(x >> 1);
}

/* set bit v in x.  v is offset from 0. */
static inline u32_t
__bitmap_set(u32_t x, int v)
{
	return x | (1<<v);
}

static inline int
__bitmap_check(u32_t x, int v)
{
	return x & (1<<v);
}

static inline u32_t
__bitmap_unset(u32_t x, int v)
{
	return x & ~(1<<v);
}

static inline void
bitmap_set(u32_t *x, int v)
{
	int idx, off;
	idx = v/WORD_SIZE; 		/* WORD_SIZE = sizeof(u32_t) */
	off = v & (WORD_SIZE-1);      
	x[idx] = __bitmap_set(x[idx], off);
}

static inline int
bitmap_check(u32_t *x, int v)
{
	int idx, off;
	idx = v/WORD_SIZE; 		/* WORD_SIZE = sizeof(u32_t) */
	off = v & (WORD_SIZE-1);      
	return __bitmap_check(x[idx], off);
}

static inline void
bitmap_unset(u32_t *x, int v)
{
	int idx, off;
	idx = v/WORD_SIZE; 		/* WORD_SIZE = sizeof(u32_t) */
	off = v & (WORD_SIZE-1);      
	x[idx] = __bitmap_unset(x[idx], off);
}

/* find the least significant one set.  max is the maximum number of
 * u32_ts in the bitmap. */
static inline int
bitmap_ls_one(u32_t *x, int max)
{
	int i, order;

	for (i = 0 ; i < max ; i++) {
		if (!x[i]) continue;
		order = ls_one(x[i]);
		return (i * WORD_SIZE) + order - 1;
	}
	return -1;
}

/* Start looking for the least significant bit at an offset (in bits)
 * into the bitmap.  The maximum size is offset from x (in u32_ts) */
static inline int 
bitmap_ls_one_offset(u32_t *x, int off, int max)
{
	int subword = WORD_SIZE - (off&(WORD_SIZE-1)), words = off/WORD_SIZE, i, ret;
	
	/* do we have an offset into a word? */
	if (subword != WORD_SIZE) {
		words++;
		if (words > max) return -1;
		for (i = 0 ; i < subword ; i++) {
			if (bitmap_check(x, off + i)) return off + i;
		}
	}
	ret = bitmap_ls_one(x+words, max-words);
	if (ret != -1) ret += (words*WORD_SIZE);
	return ret;
}

/* TODO */
static inline int
bitmap_ls_contiguous_ones(u32_t *x, int off, int span, int max)
{
	return 0;
}

#endif /* BITMAP_H */

#pragma once

#include <cos_types.h>

#define GEN_ATOMICS(type, width)				 \
static inline int cas##width(type *val, type expected, type new) \
{								\
	if (*val == expected) {					\
		*val = new;					\
		return 1;					\
	}							\
	return 0;						\
}								\
								\
static inline type						\
faa##width(type *val, long mod)					\
{								\
	type ret = *val;					\
								\
	*val += mod;						\
								\
	return ret;						\
}								\
								\
static inline type						\
load##width(type *addr)						\
{								\
	return *(volatile type *)addr;				\
}

GEN_ATOMICS(u8_t,    8)
GEN_ATOMICS(u16_t,   16)
GEN_ATOMICS(u32_t,   32)
GEN_ATOMICS(u64_t,   64)
GEN_ATOMICS(uword_t, _w)

static inline void
mem_barrier(void)
{
	return;
}

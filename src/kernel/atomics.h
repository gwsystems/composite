#pragma once

#include <cos_types.h>

static inline int
cas8(u8_t *val, u8_t expected, u8_t new)
{
	if (*val == expected) {
		*val = new;
		return 1;
	}
	return 0;
}

static inline int
cas16(u16_t *val, u16_t expected, u16_t new)
{
	if (*val == expected) {
		*val = new;
		return 1;
	}
	return 0;
}

static inline int
cas64(u64_t *val, u64_t expected, u64_t new)
{
	if (*val == expected) {
		*val = new;
		return 1;
	}
	return 0;
}

static inline u64_t
faa(u64_t *val, long mod)
{
	u64_t ret = *val;

	*val += mod;

	return ret;
}

static inline u64_t
load64(u64_t *addr)
{
	return *addr;
}

static inline void
mem_barrier(void)
{
	return;
}

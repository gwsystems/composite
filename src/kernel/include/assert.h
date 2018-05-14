#ifndef ASSERT_H
#define ASSERT_H

#include "chal.h"
#include "chal/cpuid.h"
#include "cc.h"

/* A not so nice way of oopsing */
#define die(fmt, ...)                                            \
	do {                                                     \
		printk("(%d) " fmt, get_cpuid(), ##__VA_ARGS__); \
		chal_khalt();                                    \
	} while (0)

#ifdef assert
#error "Assert in kernel already mysteriously defined."
#endif
#define assert(x)                                                                                           \
	do {                                                                                                \
		if (unlikely(0 == (x)))                                                                     \
			die("Assertion '%s' failed at %s:%d in fn %s\n", #x, __FILE__, __LINE__, __func__); \
	} while (0)

#endif /* ASSERT_H */

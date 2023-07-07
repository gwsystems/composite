#ifndef ASSERT_H
#define ASSERT_H

#include <chal.h>
#include <consts.h>
#include <types.h>
#include <compiler.h>
#include <state.h>

/* A not so nice way of oopsing */
#define die(fmt, ...)                                            \
	do {                                                     \
		printk("(%d) " fmt, coreid(), ##__VA_ARGS__); \
		chal_khalt();                                    \
	} while (0)

#ifndef COS_ASSERTIONS_ACTIVE
#define assert(x)                                                                                           \
	do {                                                                                                \
		if (unlikely(0 == (x)))                                                                     \
			die("Assertion '%s' failed at %s:%d in fn %s\n", #x, __FILE__, __LINE__, __func__); \
	} while (0)
#else
#define assert(x)
#endif

#endif /* ASSERT_H */

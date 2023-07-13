#pragma once

#include <chal.h>
#include <compiler.h>
#include <state.h>

/* A not so nice way of oopsing */
#define die(fmt, ...)                                            \
	do {                                                     \
		printk("(%d)" fmt, coreid(), ##__VA_ARGS__);	 \
		chal_khalt();                                    \
	} while (0)

#define panic(msg) die(" %s", msg)

#define assert(x)							\
	do {								\
		if (unlikely(0 == (x)))					\
			die("Assertion '%s' failed at %s:%s:%d.\n", #x, __FILE__, __func__, __LINE__); \
	} while (0)

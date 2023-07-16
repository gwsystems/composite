#pragma once

#include <chal_regs.h>
#include <chal.h>
#include <compiler.h>
#include <chal_state.h>

/* A not so nice way of oopsing */
#define die_reg(reg, fmt, ...)					 \
	do {                                                     \
		printk("(%d)" fmt, coreid(), ##__VA_ARGS__);	 \
		printk(COS_REGS_PRINT_ARGS(reg));		 \
		chal_khalt();					 \
	} while (0)

#define die(fmt, ...) die_reg(current_registers(), fmt, ##__VA_ARGS__)

#define panic(msg, reg) die_reg(reg, " %s", msg)

#define assert(x)							\
	do {								\
		if (unlikely(0 == (x)))					\
			die("Assertion '%s' failed at %s:%s:%d.\n", #x, __FILE__, __func__, __LINE__); \
	} while (0)

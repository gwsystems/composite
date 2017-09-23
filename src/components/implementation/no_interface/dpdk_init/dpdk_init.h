#ifndef DPDK_INIT_H
#define DPDK_INIT_H

#include <stdio.h>
#include <string.h>

#include <cos_debug.h>
#include <llprint.h>
#include "pci.h"

#undef assert
/* On assert, immediately switch to the "exit" thread */
#define assert(node)                                       \
	do {                                               \
		if (unlikely(!(node))) {                   \
			debug_print("assert error in @ "); \
            SPIN();                             \
		}                                          \
	} while (0)

#define SPIN()            \
	do {              \
		while (1) \
			; \
	} while (0)

#include <cos_component.h>
#include <cobj_format.h>
#include <cos_kernel_api.h>

extern struct cos_compinfo dpdk_init_info;

#endif /* MICRO_BOOTER_H */

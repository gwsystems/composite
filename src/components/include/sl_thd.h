/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2017, The George Washington University
 * Author: Gabriel Parmer, gparmer@gwu.edu
 */

#ifndef SL_THD_H
#define SL_THD_H

typedef enum {
	SL_THD_FREE = 0,
	SL_THD_BLOCKED,
	SL_THD_BLOCKED_TIMEOUT,
	SL_THD_WOKEN, /* if a race causes a wakeup before the inevitable block */
	SL_THD_RUNNABLE,
	SL_THD_DYING,
} sl_thd_state;

struct sl_thd {
	sl_thd_state   state;
	thdid_t        thdid;
	thdcap_t       thdcap;
	tcap_prio_t    prio;
	struct sl_thd *dependency;

	cycles_t period;
	cycles_t periodic_cycs; /* for implicit periodic timeouts */
	cycles_t timeout_cycs;  /* next timeout - used in timeout API */
	cycles_t wakeup_cycs;   /* actual last wakeup - used in timeout API for jitter information, etc */
	int      timeout_idx;   /* timeout heap index, used in timeout API */
};

#ifndef assert
#define assert(node)                                       \
	do {                                               \
		if (unlikely(!(node))) {                   \
			debug_print("assert error in @ "); \
			*((int *)0) = 0;                   \
		}                                          \
	} while (0)
#define PRINT_FN prints
#define debug_print(str) (PRINT_FN(str __FILE__ ":" STR(__LINE__) ".\n"))
#define BUG()                          \
	do {                           \
		debug_print("BUG @ "); \
		*((int *)0) = 0;       \
	} while (0);
#define SPIN()            \
	do {              \
		while (1) \
			; \
	} while (0)
#endif

#endif /* SL_THD_H */

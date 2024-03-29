#ifndef QUANTUM_H
#define QUANTUM_H

#include <slm.h>

SLM_MODULES_TIMER_PROTOTYPES(quantum)

struct slm_timer_thd {
	int      timeout_idx;	/* where are we in the heap? */
	cycles_t abs_wakeup;
};

#endif	/* QUANTUM_H */

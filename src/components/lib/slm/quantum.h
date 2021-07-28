#ifndef QUANTUM_H
#define QUANTUM_H

#include <slm.h>

void slm_timer_quantum_expire(cycles_t now);
int slm_timer_quantum_add(struct slm_thd *t, cycles_t absolute_timeout);
int slm_timer_quantum_cancel(struct slm_thd *t);
int slm_timer_quantum_thd_init(struct slm_thd *t);
void slm_timer_quantum_thd_deinit(struct slm_thd *t);
int slm_timer_quantum_init(void);

struct slm_timer_thd {
	int      timeout_idx;	/* where are we in the heap? */
	cycles_t abs_wakeup;
};

#endif	/* QUANTUM_H */

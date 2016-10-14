/* Implementation for the bmk spl priority functions */

#include "cos_sync.h"
#include <cos_kernel_api.h>
#include <cos_types.h>

volatile isr_state_t cos_isr = 0;  /* Last running isr thread */
unsigned int cos_nesting = 0; 	   /* Depth to intr_disable/intr_enable */
u32_t intrs = 0; 	           /* Intrrupt bit mask */

extern volatile thdcap_t cos_cur; /* Last running rk thread */

/* Called from cos_irqthd_handler */

static inline void 
isr_setcontention(unsigned int intr) 
{ 
	/* intr is a thdcap_t */
	isr_state_t tmp, final;

	do {
		unsigned int rk_disabled;
		unsigned int intr_disabled;
		thdcap_t contending;

		tmp = cos_isr;
		isr_get(tmp, &rk_disabled, &intr_disabled, &contending);	

		contending = intr;
		
		final = isr_construct(rk_disabled, intr_disabled, contending);
	} while (unlikely(!ps_cas((unsigned long *)&cos_isr, tmp, final)));
}

/* cos_intrdisabled should be the second bit highest bit in cos_isr*/



void
intr_start(thdcap_t thdcap)
{
	isr_state_t tmp, final;
	int ret;

	assert(thdcap);

	/*
	 * 1. Get current cos_isr
         * 2. Check if intr_disabled is set (another isr thread is unblocked)
	 *    YES: switch main thread, after goto 1, recheck
	 * 3. Set contention to us / set intr_disabled
	 * 4. Check if rk_disabled
	 *    YES: switch to last running rk thread
	 *    NO : Run interrupt
	 */
	while (1) {

		unsigned int rk_disabled;
		unsigned int intr_disabled;
		thdcap_t contending;

		do {
			/* 1. */
again:
			tmp = cos_isr;
			isr_get(tmp, &rk_disabled, &intr_disabled, &contending);

			/* 2. */
			if (intr_disabled) {
				assert(contending);
				assert(contending != thdcap); /* Make sure we are not trying to switch to ourself */
				/* Switch to contending isr thread */
				do {
                        		ret = cos_switch(contending, cos_cur_tcap, rk_thd_prio, 
							TCAP_TIME_NIL, BOOT_CAPTBL_SELF_INITRCV_BASE, 
							cos_sched_sync());
                        		assert (ret == 0 || ret == -EAGAIN);
                		} while(ret == -EAGAIN);
				goto again;
			}

			/* 3. */
			contending = thdcap;
			intr_disabled = 1;

			final = isr_construct(rk_disabled, intr_disabled, contending);
		} while (unlikely(!ps_cas((unsigned long *)&cos_isr, tmp, final)));
		

		/* Comitting to only one isr thread running now, we have set intr_disabled */

		/* 4. */
		if (unlikely(rk_disabled)) {
			/* Unset intr_disabled so we can let the rk run */
			cos_isr = isr_construct(rk_disabled, 0, contending);
			do {
				/* Switch back to RK thread */
				ret = cos_switch(cos_cur, COS_CUR_TCAP,
						rk_thd_prio, TCAP_TIME_NIL,
						BOOT_CAPTBL_SELF_INITRCV_BASE,
						cos_sched_sync());
				assert(ret == 0 || ret == -EAGAIN);
			} while (ret == -EAGAIN);
			goto again;
		}

		/* Ready to run interrupt, better make sure that interrupts are enabled by now */
		assert(!cos_nesting);
		assert(!(cos_isr>>31));
		assert(((cos_isr>>30) & 1));

		return;
	}
}

/* Called from cos_irq_handler */
void
intr_end(void)
{
	assert(!cos_nesting); 
	assert(!(cos_isr>>31));
	assert(((cos_isr>>30) & 1));
	cos_isr = isr_construct(0, 0, 0);
	assert(!cos_isr);
}

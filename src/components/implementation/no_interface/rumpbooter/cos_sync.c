/* Implementation for the bmk spl priority functions */

#include "cos_sync.h"
#include <cos_kernel_api.h>
#include <cos_types.h>

volatile isr_state_t cos_isr = 0;  /* Last running isr thread */
unsigned int cos_nesting = 0; 	   /* Depth to intr_disable/intr_enable */
u32_t intrs = 0; 	           /* Intrrupt bit mask */

extern volatile thdcap_t cos_cur; /* Last running rk thread */
volatile unsigned int cos_intrdisabled = 0; /* Variable to detect if cos interrupt threads disabled interrupts */

/* Called from cos_irqthd_handler */

static inline void 
isr_setcontention(unsigned int intr) 
{ 
	/* intr is a thdcap_t */
	isr_state_t tmp, final;

	do {
		unsigned int isdisabled;
		thdcap_t contending;

		tmp = cos_isr;
		isr_get(tmp, &isdisabled, &contending);	

		contending = intr;
		
		final = isr_construct(isdisabled, contending);
	} while (unlikely(!ps_cas((unsigned long *)&cos_isr, tmp, final)));
}



void
intr_start(thdcap_t thdcap)
{
	isr_state_t tmp, final;
	int ret;

	assert(thdcap);

	/*
	 * 1. if cos isr thread disabled interrupts goto contending (last cos_isr thread)
	 * 2. check disabled
	 * 3. set contended 	
	 * 4a. if rk disabled goto cos_cur and retry when we return
	 * 4c. if enabled  return and process intrerrupt
	 */
	while (1) {
		do {
			unsigned int isdisabled;
			thdcap_t contending;

			tmp = cos_isr;
			isr_get(tmp, &isdisabled, &contending);

			/*
			 * Check if we are disabled from cos isr thread and if contending is set
			 * or check rare case where contention has not yet been set, but contending has AND
			 * we know a rk has not enabled interrupts
			 * Rare case can happen if cos isr thread is prempted below in cos_intrdisabled cas loop
			 */
			if (cos_intrdisabled || (contending && !(int)isdisabled)) {
				/* If a cos isr thread disabled interrupts we better have contending set */
				assert(contending);
				do {
					/* FIXME this should not use the cos_cur_tcap */
					ret = cos_switch(contending, cos_cur_tcap, rk_thd_prio, TCAP_TIME_NIL, BOOT_CAPTBL_SELF_INITRCV_BASE, cos_sched_sync());
					assert(ret == 0 || ret == -EAGAIN);
				} while (ret == -EAGAIN);
			}

			contending = thdcap;

			ret = (int)isdisabled;
			if (!isdisabled) assert(cos_nesting == 0);

			final = isr_construct(isdisabled, contending);
		} while (unlikely(!ps_cas((unsigned long *)&cos_isr, tmp, final)));

		/* If interrupts are disabled lets run this interrupt */
		if (likely(!ret)) {
			/*
			 * Cos interrupt thread sets special disabled variable
			 * After setting cos_intrdisabled, we are commiting to
			 * finish processing interrupts before going back to rk
			 */
			int first = 1;
			do {

				if (first) {
					tmp = cos_intrdisabled;
					final = tmp + 1;
				} else {
					printc("goto again\n");
					goto again;
				}
				first = 0;

			} while (unlikely(!ps_cas((unsigned long *)&cos_intrdisabled, tmp, final)));

			/* We should not be here with another interrupt having set cos_intrdisabled */
			if (cos_intrdisabled > 1) assert(0);
			return;
		}

		/* Switch back to RK thread */
		do {
			ret = cos_switch(cos_cur, cos_cur_tcap, rk_thd_prio, TCAP_TIME_NIL, BOOT_CAPTBL_SELF_INITRCV_BASE, cos_sched_sync());
			assert(ret == 0 || ret == -EAGAIN);
		} while (ret == -EAGAIN);

again: /* Loop again */ ;
	}
}

/* Called from cos_irq_handler */
void
intr_end(void)
{
	/* cos interrupt thread unsets special disabled variable */
	assert(cos_intrdisabled);
	__sync_fetch_and_sub(&cos_intrdisabled, 1);

	assert(!cos_nesting && !(cos_isr>>31)); 
	isr_setcontention(0);
	assert(!cos_isr);

}

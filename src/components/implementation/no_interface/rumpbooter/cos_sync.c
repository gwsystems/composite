/* Implementation for the bmk spl priority functions */

#include "cos_sync.h"
#include <cos_kernel_api.h>
#include <cos_types.h>
#include "vk_types.h"
#include "rumpcalls.h"

volatile isr_state_t cos_isr = 0;  /* Last running isr thread */
unsigned int cos_nesting = 0;	   /* Depth to intr_disable/intr_enable */
u32_t intrs = 0;	           /* Intrrupt bit mask */

extern volatile thdcap_t cos_cur; /* Last running rk thread */

/* Called from cos_irqthd_handler */

static inline void
isr_setcontention(unsigned int intr)
{
	/* intr is a irq line number */
	isr_state_t tmp, final;

	do {
		unsigned int rk_disabled;
		unsigned int intr_disabled;
		unsigned int contending;

		tmp = cos_isr;
		isr_get(tmp, &rk_disabled, &intr_disabled, &contending);

		contending = intr;

		final = isr_construct(rk_disabled, intr_disabled, contending);
	} while (unlikely(!ps_cas((unsigned long *)&cos_isr, tmp, final)));
}

/* cos_intrdisabled should be the second bit highest bit in cos_isr*/

void
intr_start(unsigned int irqline)
{
	isr_state_t tmp, final;
	int ret;

	assert(irqline >= HW_ISR_FIRST && irqline < HW_ISR_LINES);
	if (cos_spdid_get()) assert(irqline == IRQ_DOM0_VM);
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
		unsigned int contending;

		do {
			/* 1. */
			tmp = cos_isr;
			isr_get(tmp, &rk_disabled, &intr_disabled, &contending);

			/* 2. */
			if (intr_disabled) {
				tcap_prio_t prio;

				assert(contending >= HW_ISR_FIRST && contending < HW_ISR_LINES);
				assert(contending != irqline); /* Make sure we are not trying to switch to ourself */

				/* Switch to contending isr thread */
				do {
					//ret = cos_switch(irq_thdcap[contending], intr_eligible_tcap(contending),
					//		 irq_prio[contending], TCAP_TIME_NIL,
					//		 BOOT_CAPTBL_SELF_INITRCV_BASE, cos_sched_sync());
					ret = cos_thd_switch(irq_thdcap[contending]);
					assert (ret == 0 || ret == -EAGAIN);
				} while(ret == -EAGAIN);

				continue;
			}

			/* 3. */
			contending = irqline;
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
				//ret = cos_switch(cos_cur, COS_CUR_TCAP, rk_thd_prio, TCAP_TIME_NIL,
				//		 BOOT_CAPTBL_SELF_INITRCV_BASE, cos_sched_sync());
				ret = cos_thd_switch(cos_cur);
				assert(ret == 0 || ret == -EAGAIN);
			} while (ret == -EAGAIN);

			continue;
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

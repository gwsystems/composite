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
	} while (unlikely(!ps_cas(&cos_isr, tmp, final)));
}



void
intr_start(thdcap_t thdcap)
{
	isr_state_t tmp, final;
	int ret;

	assert(thdcap);

	/*
	 * 1. check disabled
	 * 2. set contended 	
	 * 3a. if disabled goto cos_cur and retry when we return
	 * 3b. if enabled  return and process intrerrupt
	 */
	while(1) {
		do {
			int isdisabled;
			thdcap_t contending;

			tmp = cos_isr;
			isr_get(tmp, &isdisabled, &contending);

			contending = thdcap;

			ret = isdisabled;
			if (!isdisabled) assert(cos_nesting == 0);

			final = isr_construct(isdisabled, contending);
		} while (unlikely(!ps_cas(&cos_isr, tmp, final)));

		if (likely(!ret)) {
			/*
			 * Cos interrupt thread sets special disabled variable
			 * After settting cos_intrdisabled, we are commiting to
			 * finish processing interrupts before going back to rk
			 */
			__sync_fetch_and_add(&cos_intrdisabled, 1);
			if(cos_intrdisabled > 1) printc("cos_intrdisabled: %d, intrs: %b\n", cos_intrdisabled, intrs);
			assert(cos_intrdisabled);
			return 0;
		}

		do {
			ret = cos_switch(cos_cur, BOOT_CAPTBL_SELF_INITTCAP_BASE, TCAP_PRIO_MAX, TCAP_TIME_NIL, BOOT_CAPTBL_SELF_INITRCV_BASE, cos_sched_sync());
			assert(ret == 0 || ret == -EAGAIN);
		} while (ret == -EAGAIN);
	}
}

/* Called from cos_irq_handler */
void
intr_end(void)
{
	assert(!cos_nesting && !(cos_isr>>31)); 
	isr_setcontention(0);
	assert(!cos_isr);

	/* cos interrupt thread unsets special disabled variable */
	assert(cos_intrdisabled);
	__sync_fetch_and_sub(&cos_intrdisabled, 1);

}

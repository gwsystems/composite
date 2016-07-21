/* Implementation for the bmk spl priority functions */

#include "cos_sync.h"
#include <cos_kernel_api.h>
#include <cos_types.h>

#include <ps_plat_linux.h>

u32_t intrs = 0; 	        /* Intrrupt bit mask */
extern volatile thdcap_t cos_cur;     /* Last running rk thread */

/* Called from cos_irq_handler */

static inline void 
isr_setcontention(unsigned int intr) 
{ 
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

static unsigned int
intr_translate_thdid2irq(thdid_t tid)
{
	int i = 0;

	if(tid == 0) return -1;

	while(tid != irq_thdid[i] && i < 32) i++;
	/* Make sure that we are dealing with an irq thread id*/
	assert(i != 32);

	return i;
}

static inline void
intr_update(unsigned int irq_line, int rcving)
{
	/* if an event for not an irq_line */
	if(irq_line == -1) return;

	/* blocked, unset intterupt to be worked on */
	if(rcving) intrs &= !(1<<(irq_line-1));
	/* unblocked, set intterupt to be worked on */
	else intrs |= 1<<(irq_line-1);
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
	 */

	do {
		int isdisabled;
		thdcap_t contending;

		tmp = cos_isr;
		isr_get(tmp, &isdisabled, &contending);

		contending = thdcap;
		ret = isdisabled;
		if (!isdisabled) assert(nesting == 0);

		final = isr_construct(isdisabled, contending);
	} while (unlikely(!ps_cas(&cos_isr, tmp, final)));

	if (likely(!ret)) return 0;

	do {
		ret = cos_switch(cos_cur, 0, 0, 0, BOOT_CAPTBL_SELF_INITRCV_BASE);
		assert(ret == 0 || ret == -EAGAIN);
	} while (ret == -EAGAIN);
}

/* Called from cos_irq_handler */
void
intr_end(void)
{
	assert(!nesting && !(cos_isr>>31)); 
	isr_setcontention(0);
}


/* TODO: Should this be a separate API */
void
event_process(int pending, int tid, int rcving)
{
        cycles_t cycles;
	int i;
	int irq_line; /* Offset into irq_thdcap for the right interrupt thdcap */
	int ret;

	irq_line = intr_translate_thdid2irq(tid); /* Analogous to the "which" argument in the irq_handler function */

	intr_update(irq_line, rcving);

	i = 0;
	for(; i < pending ; i++) {
		/* Get the next event */
		cos_rcv(BOOT_CAPTBL_SELF_INITRCV_BASE, &tid, &rcving, &cycles);

		irq_line = intr_translate_thdid2irq(tid);
		intr_update(irq_line, rcving);
	}

	/* Done processing pending events. Finish any remaining interrupts */
	/* TODO, see if we need to disable interupts here */

	i = 32;
	for(; i > 0 ; i--) {
		int tmp = intrs;

		if((tmp>>(i-1)) & 1) {
			ret = cos_switch(irq_thdcap[i], 0, 0, 0, BOOT_CAPTBL_SELF_INITRCV_BASE);
			if(ret) printc("intr_pending, FAILED cos_switch %d\n", ret);
		}
	}
}

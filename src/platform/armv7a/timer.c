#include "cav7_consts.h"


#define CHAL_CYC_THRESH (CYC_PER_USEC * 100)
void
chal_timer_set(cycles_t cycles)
{
	cycles_t now;
	rdtscll(now);
	if (now < cycles) {
		cycles -= now;
	} else {
		/* in the past? set to fire one tick from now! */
		cycles  = CHAL_CYC_THRESH;
	}
	chal_timer_disable();
	/* Writing this will also write the counter register as well */
	CAV7_PTWD_PTLR = cycles / 2;
	/* CAV7_PTWD_PTCNTR = cycles/2; */
	/* Clear the interrupt flag - write 1! */
	CAV7_PTWD_PTISR = 1;
	/* Ack all timer interrupts */
	CAV7_GICC_EOIR = 29;
	/* Enable the interrupt */
	CAV7_GICD_ISENABLER(0) = 1 << 29;
	/* Start the timer */
	CAV7_PTWD_PTCTLR = CAV7_PTWD_PTCTLR_PRESC(0) | CAV7_PTWD_PTCTLR_IRQEN | CAV7_PTWD_PTCTLR_TIMEN;
}

void
chal_timer_disable(void)
{
	/* Disable the interrupt */
	CAV7_GICD_ICENABLER(0) = 0 << 29;
	CAV7_PTWD_PTCTLR       = 0;
	/* Clear the interrupt flag - write 1! */
	CAV7_PTWD_PTISR = 1;
}

unsigned int
chal_cyc_thresh(void)
{
	return CHAL_CYC_THRESH;
}

void
pmc_ready(void)
{
	/* enable user-mode access to the performance counter*/
	asm ("MCR p15, 0, %0, C9, C14, 0\n\t" :: "r"(1)); 

	/* disable counter overflow interrupts (just in case)*/
	asm ("MCR p15, 0, %0, C9, C14, 2\n\t" :: "r"(0x8000000f));
}

void
timer_init(void)
{
	/* We are initializing the global timer here */
	CAV7_GTMR_GTCNTRL = 0;
	CAV7_GTMR_GTCNTRH = 0;
	CAV7_GTMR_GTCTLR  = 1;
	printk("global timer init\n");
	pmc_ready();
}

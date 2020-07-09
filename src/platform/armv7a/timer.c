#include "cav7_consts.h"


#define CHAL_CYC_THRESH 76700
void
chal_timer_set(cycles_t cycles)
{
	chal_timer_disable();
	// cycles = 7670*100;
	// printk("cycles %lld\n", cycles);
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

/*
 * https://blog.regehr.org/archives/794
 * Code copied from here: it is required to enable user-land access to the cycle counter.
 * Lets see if this works!
 */
void
cycle_counter_user_enable(void)
{
	__asm__ __volatile__ ("mcr p15,  0, %0, c15,  c9, 0\n" : : "r" (1));
	printk ("User-level access to CCR has been turned on.\n");
}

void
timer_init(void)
{
	/* We are initializing the global timer here */
	CAV7_GTMR_GTCNTRL = 0;
	CAV7_GTMR_GTCNTRH = 0;
	CAV7_GTMR_GTCTLR  = 1;
	printk("global timer init\n");
	cycle_counter_user_enable();
}

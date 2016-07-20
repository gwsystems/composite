/* Implementation for the bmk spl priority functions */

#include "cos_lock.h"
#include <cos_kernel_api.h>
#include <cos_types.h>

static void intr_reset(int *intr);
static void intr_setcontention(int intr);
static void intr_update(int irq_line, int rcving);
static int intr_translate_thdid2irq(int tid);

int intrs = 0; 	        /* Intrrupt bit mask */
signed int cos_isr = 0; /* Last running isr thread */
extern int cos_cur;     /* Last running rk thread */

/* TODO: 
 * 3 macros: set highest bit, test highest bit, unset highest bit
 * Handle nesting with the remaining 15 bits not used for contention
 */

void
intr_disable(void)
{
	/* Set highest order bit in cos_isr to 1 */
	__sync_fetch_and_or(&cos_isr, 0x80000000);
}

void
intr_enable(void)
{
	int temp, ret;

	/* Set highest order bit in cos_isr to 0 */
	temp = cos_isr;
	__sync_fetch_and_and(&temp, 0x7FFFFFFF);
	/* Reset remaining bits to 0, switch to isr handler if there was contention */
	intr_reset(&cos_isr);
	if(temp > 0) {

		/* 
		 * TODO
		 * If we get prempted here there is a chance that cos_resume handled the interupt and
		 * not us. How should we check for this???
		 */

		printc("About to thd switch at intr_enable to: %d\n", temp);
		ret = cos_switch(temp, 0, 0, 0, BOOT_CAPTBL_SELF_INITRCV_BASE, cos_sched_sync());	
		if(ret) printc("intr_enable, FAILED cos_switch %s\n", strerror(ret));
	}
}

static void
intr_reset(int *intr)
{
	*intr = 0;
}

int
intr_getdisabled(int intr)
{
	/* Returns 1 if interupts are disabled */
	return intr>>31;
}

static void
intr_setcontention(int intr)
{
	/* Set first 16 bits to be the isr thread we will want to eventually switch to */
	__sync_fetch_and_or(&cos_isr, intr);
}

void
intr_delay(int isrthd)
{
	int temp, ret;
	/* 
	 * Called from isr handler
	 * Check if interupts are disabled, if not return and wake up isr thread
	 */
	while(1) {
		temp = cos_isr;
		if(intr_getdisabled(temp)) {
			if(temp == cos_isr) {
				intr_setcontention(isrthd);
				printc("About to thd switch at intr_delay to: %d\n", cos_cur);
				ret = cos_switch(cos_cur, 0, 0, 0, BOOT_CAPTBL_SELF_INITRCV_BASE, cos_sched_sync());	
				if(ret) printc("intr_delay, FAILED cos_switch %s\n", strerror(ret));
			} /* else return back to top of loop */
		} else break; 
	}
}

void
intr_pending(int pending, int tid, int rcving)
{	
        cycles_t cycles;
	int i;
	int irq_line;
	int ret;
	
	irq_line = intr_translate_thdid2irq(tid); /* Analogous to the "which" argument in the irq_handler function */

	intr_update(irq_line, rcving);

	i = 0; 
	for(; i < pending; i++) {
		/* Get the next event */
		cos_rcv(BOOT_CAPTBL_SELF_INITRCV_BASE, &tid, &rcving, &cycles);
		irq_line = intr_translate_thdid2irq(tid);

		intr_update(irq_line, rcving);		
	}

	/* Done processing pending events. Finish any remaining interrupts */
	/* TODO, see if we need to disable interupts here */

	i = 32;
	for(; i > 0; i--) {
		int temp = intrs;
		if((temp>>(i-1)) & 1) {
			printc("About to thd switch in intr_pending to: %d\n", irq_thdcap[i]);
			ret = cos_switch(irq_thdcap[i], 0, 0, 0, BOOT_CAPTBL_SELF_INITRCV_BASE, cos_sched_sync());	
			if(ret) printc("intr_pending, FAILED cos_switch %s\n", strerror(ret));
		}
	}
}

static void
intr_update(int irq_line, int rcving)
{
	/* if an event for not an irq_line */
	if(irq_line == -1) return;

	if(irq_line && rcving) {
		/* blocked, unset intterupt to be worked on */	
		intrs &= !(1<<(irq_line-1));
	} else if(irq_line && !rcving) {
		/* unblocked, set intterupt to be worked on */
		intrs |= 1<<(irq_line-1);
	}
}

static int
intr_translate_thdid2irq(int tid)
{
	int i = 0;

	if(tid == 0) return -1;
	
	while(tid != irq_thdid[i] && i < 32) i++;
	/* Make sure that we are dealing with an irq thread id*/
	assert(i != 32); 

	return i;
}

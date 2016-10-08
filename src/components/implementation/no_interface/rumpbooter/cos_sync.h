#ifndef COS_SYNC_H
#define COS_SYNC_H

#include <cos_types.h>
#include <cos_kernel_api.h>
#include "rumpcalls.h"

//#define assert(node)if (unlikely(!(node))) { debug_print("assert error in @ "); while(1);}

typedef u32_t isr_state_t;
extern volatile isr_state_t cos_isr;           /* Last running isr thread */
extern unsigned int cos_nesting;               /* Depth to intr_disable/intr_enable */
extern u32_t intrs; 	                       /* Intrrupt bit mask */
extern volatile unsigned int cos_intrdisabled; /* Variable to detect if cos interrupt threads disabled interrupts */
extern struct cos_compinfo booter_info;

extern volatile tcap_t cos_cur_tcap;

int __attribute__((format(printf,1,2))) printc(char *fmt, ...);

int  intr_getdisabled(int intr);
void intr_start(thdcap_t tid);
void intr_end(void);

#define PS_ATOMIC_POSTFIX "l" /* x86-32 */
#define PS_CAS_INSTRUCTION "cmpxchg"
#define PS_CAS_STR PS_CAS_INSTRUCTION PS_ATOMIC_POSTFIX " %2, %0; setz %1"

extern int vmid;

/*
 * Return values:
 * 0 on failure due to contention (*target != old)
 * 1 otherwise (*target == old -> *target = updated)
 */
static inline int
ps_cas(unsigned long *target, unsigned long old, unsigned long updated)
{

        char z;
        __asm__ __volatile__("lock " PS_CAS_STR
                             : "+m" (*target), "=a" (z)
                             : "q"  (updated), "a"  (old)
                             : "memory", "cc");
        return (int)z;
}

static inline tcap_t
intr_translate_thdcap2irqtcap(thdcap_t tc)
{
	int i = HW_ISR_FIRST;

	if (tc == 0) return 0;

	if (vmid) {
		if (tc == irq_thdcap[IRQ_DOM0_VM])
			return irq_tcap[IRQ_DOM0_VM]; /* which is BOOT_CAPTBL_SELF_INITTCAP_BASE */
		return 0;
	}

	while (tc != irq_thdcap[i] && i < HW_ISR_LINES) i ++;

	if (i >= HW_ISR_LINES) return 0;

	return irq_tcap[i];
}

static inline int
intr_translate_thdcap2irq(thdcap_t tc)
{
	int i = HW_ISR_FIRST;

	if (tc == 0) return -1;

	if (vmid) {
		if (tc == irq_thdcap[IRQ_DOM0_VM])
			return IRQ_DOM0_VM;
		return -1;
	}

	while (tc != irq_thdcap[i] && i < HW_ISR_LINES) i ++;

	if (i >= HW_ISR_LINES) return -1;

	return i;
}

static unsigned int
intr_translate_thdid2irq(thdid_t tid)
{
	int i = HW_ISR_FIRST;

	if(tid == 0) return -1;

	if (vmid) {
		if(tid == irq_thdid[IRQ_DOM0_VM])
			return IRQ_DOM0_VM;
		else return -1;
	}

	while(tid != irq_thdid[i] && i < HW_ISR_LINES) i++;
	/* Make sure that we are dealing with an irq thread id*/
	if(i >= HW_ISR_LINES) return -1;

	return i;
}

static inline tcap_t
intr_real_irq_rcv(void)
{
	int i = HW_ISR_FIRST;

	/* only in DOM0 */
	assert(vmid == 0);

	/* should not be executed unless vm1 or vm2 is using line 1 */
	while (i == IRQ_VM1 || i == IRQ_VM2) i ++;
	assert(i < HW_ISR_LINES);
	/* for now all real I/O use same tcap */
	return irq_arcvcap[i];
}

static inline tcap_t
intr_eligible_tcap(thdcap_t irqtc)
{
	tcap_res_t irqbudget, initbudget;
	tcap_t tc;
	int irqline, ret;

	assert (irqtc);

	if (vmid) {
		if (irqtc == irq_thdcap[IRQ_DOM0_VM])
			return irq_tcap[IRQ_DOM0_VM];
		assert(0);
	}

	tc = intr_translate_thdcap2irqtcap(irqtc);
	assert(tc);
	irqline = intr_translate_thdcap2irq(irqtc);
	assert(irqline >= 0);

	irqbudget = (tcap_res_t)cos_introspect(&booter_info, tc, TCAP_GET_BUDGET);
	if (irqbudget == 0) {
		if (irqline == IRQ_VM1 || irqline == IRQ_VM2) {
			//assert(0);
			printc("-%d", irqline);
/*			initbudget = (tcap_res_t)cos_introspect(&booter_info, BOOT_CAPTBL_SELF_INITTCAP_BASE, TCAP_GET_BUDGET);	
			if ((ret = cos_tcap_transfer(irq_arcvcap[irqline], BOOT_CAPTBL_SELF_INITTCAP_BASE, initbudget / 2, irq_prio[irqline]))) {
				printc("Irq %d Tcap transfer failed %d\n", irqline, ret);
				assert(0);
			}
*/
		} else {
			initbudget = (tcap_res_t)cos_introspect(&booter_info, BOOT_CAPTBL_SELF_INITTCAP_BASE, TCAP_GET_BUDGET);
			if ((ret = cos_tcap_transfer(irq_arcvcap[irqline], BOOT_CAPTBL_SELF_INITTCAP_BASE, initbudget / 2, irq_prio[irqline]))) {
				printc("Irq %d Tcap transfer failed %d\n", irqline, ret);
				assert(0);
			}
		}
	}
	
	return tc;
}

static inline void
intr_update(unsigned int irq_line, int rcving)
{
	/* if an event for not an irq_line */
	if((int)irq_line == -1) return;

	assert(irq_line);

	/* blocked, unset intterupt to be worked on */
	if(rcving) {
		intrs &= ~(1<<(irq_line-1));
	}
	/* unblocked, set intterupt to be worked on */
	else {
		intrs |= 1<<(irq_line-1);
	}
}

static inline void
isr_get(isr_state_t tmp, unsigned int *isdisabled, thdcap_t *contending)
{
	*isdisabled = tmp >> 31;
	*contending = (thdcap_t)(tmp & ((u32_t)(~0) >> 16));
	assert(*contending < (1 << 16));
}

static inline isr_state_t
isr_construct(unsigned int isdisabled, thdcap_t contending)
{
	assert(isdisabled <= 1 && contending < (1 << 16));
	return (u32_t)isdisabled << 31 | (u32_t)contending;
}

/* Set highest order bit in cos_isr to 1 or increment cos_nesting count and return */
static inline void
isr_disable(void)
{
	isr_state_t tmp, final;

	/* Isr is currently enabled, disable for first time */
	if(!cos_nesting) {
		do {
			unsigned int isdisabled;
			thdcap_t contending;

			tmp = cos_isr;
			isr_get(tmp, &isdisabled, &contending);	
			/*
			 * Intrrupts must be enabled and cos_nesting must be 0
			 * Cannot have enabled and contentended.  This would imply
			 * that a non-interrupt thread has preempted an interrupt
			 * thread.
			 */
			assert(!isdisabled);
			assert(!cos_nesting);
			final = isr_construct(1, contending);
		} while (unlikely(!ps_cas((unsigned long *)&cos_isr, tmp, final)));
	}
		
	cos_nesting++;
}

static inline thdcap_t
isr_enable(void)
{
	isr_state_t tmp, final;
	thdcap_t contending = 0;

	/* We better have disabled before calling enable */
	assert(cos_nesting);
	cos_nesting--;

	/* Actually enable interrupts */
	if(!cos_nesting) {
		do {
			unsigned int isdisabled;

			tmp = cos_isr;
			isr_get(tmp, &isdisabled, &contending);
			assert(isdisabled);

			/* no more cos_nesting, actually "reenable" interrupts */
			final = isr_construct(0, 0);

		} while (unlikely(!ps_cas((unsigned long *)&cos_isr, tmp, final)));
	}

	return contending;
}

static inline void
intr_disable(void)
{ isr_disable(); }

static inline void
intr_enable(void)
{
	thdcap_t contending;
	tcap_prio_t prio;
	int ret;

	contending = isr_enable();
	if (unlikely(contending && !cos_intrdisabled)) {
		int irq = intr_translate_thdcap2irq(contending);

		assert (irq >= 0);
		prio = irq_prio[irq];
		/*
		 * Assumption here: we have actually re-enabled
		 * interrupts here as opposed to release another
		 * cos_nesting of the interrupts
		 */
		do {
			ret = cos_switch(contending, intr_eligible_tcap(contending), prio, TCAP_TIME_NIL, BOOT_CAPTBL_SELF_INITRCV_BASE, cos_sched_sync());
			assert (ret == 0 || ret == -EAGAIN);
		} while(ret == -EAGAIN);
	}
}

#endif

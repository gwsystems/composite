#ifndef COS_SYNC_H
#define COS_SYNC_H

#include <cos_types.h>
#include <cos_kernel_api.h>
#include "rumpcalls.h"
#include "vk_types_old.h"

//#define assert(node)if (unlikely(!(node))) { debug_print("assert error in @ "); while(1);}

typedef u32_t isr_state_t;
extern volatile isr_state_t cos_isr;           /* Last running isr thread */
extern unsigned int cos_nesting;               /* Depth to intr_disable/intr_enable */
extern u32_t intrs;	                       /* Intrrupt bit mask */
extern volatile unsigned int cos_intrdisabled; /* Variable to detect if cos interrupt threads disabled interrupts */
extern struct cos_compinfo booter_info;

int __attribute__((format(printf,1,2))) printc(char *fmt, ...);

int  intr_getdisabled(int intr);
void intr_start(unsigned int irqline);
void intr_end(void);

#define PS_ATOMIC_POSTFIX "l" /* x86-32 */
#define PS_CAS_INSTRUCTION "cmpxchg"
#define PS_CAS_STR PS_CAS_INSTRUCTION PS_ATOMIC_POSTFIX " %2, %0; setz %1"

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

static unsigned int intr_translate_thdid2irq(thdid_t tid)
{
	int i = HW_ISR_FIRST;

	if(tid == 0) return -1;

	if (cos_spdid_get()) {
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
intr_eligible_tcap(unsigned int irqline)
{
#if defined(__INTELLIGENT_TCAPS__) || defined(__SIMPLE_DISTRIBUTED_TCAPS__)
	tcap_res_t irqbudget, initbudget;

	assert (irqline >= HW_ISR_FIRST && irqline < HW_ISR_LINES);

	if (cos_spdid_get()) {
		if (irqline == IRQ_DOM0_VM)
			return irq_tcap[IRQ_DOM0_VM];
		assert(0);
	}

	irqbudget = (tcap_res_t)cos_introspect(&booter_info, irq_tcap[irqline], TCAP_GET_BUDGET);
	if (irqbudget == 0) {
		cos_dom02io_transfer(irqline, irq_tcap[irqline], irq_arcvcap[irqline], irq_prio[irqline]); 
	}
	
	return irq_tcap[irqline];
#elif defined(__SIMPLE_XEN_LIKE_TCAPS__)
	return BOOT_CAPTBL_SELF_INITTCAP_BASE;
#endif
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
isr_get(isr_state_t tmp, unsigned int *rk_disabled, unsigned int *intr_disabled, unsigned int *contending)
{
	*rk_disabled = tmp >> 31;
	*intr_disabled = (tmp >> 30) & 1;
	*contending = (unsigned int)(tmp & ((u32_t)(~0) >> 16));
	assert(*contending < (1 << 16));
}

static inline isr_state_t
isr_construct(unsigned int rk_disabled, unsigned int intr_disabled, unsigned int contending)
{
	assert(rk_disabled <= 1 && intr_disabled <= 1 && contending < (1 << 16));
	return (u32_t)rk_disabled << 31 | (u32_t)intr_disabled << 30 | (u32_t)contending;
}

/* Set highest order bit in cos_isr to 1 or increment cos_nesting count and return */
static inline void
isr_disable(void)
{
	isr_state_t tmp, final;

	/* Isr is currently enabled, disable for first time */
	if(!cos_nesting) {
		do {
			unsigned int rk_disabled;
			unsigned int intr_disabled;
			unsigned int contending;

			tmp = cos_isr;
			isr_get(tmp, &rk_disabled, &intr_disabled, &contending);	
			/*
			 * Intrrupts must be enabled and cos_nesting must be 0
			 * Cannot have enabled and contentended.  This would imply
			 * that a non-interrupt thread has preempted an interrupt
			 * thread.
			 */
			assert(!rk_disabled);
			assert(!cos_nesting);
			final = isr_construct(1, intr_disabled, contending);
		} while (unlikely(!ps_cas((unsigned long *)&cos_isr, tmp, final)));
	}
		
	cos_nesting++;
}

static int isintr;

static inline unsigned int
isr_enable(void)
{
	isr_state_t tmp, final;
	unsigned int contending = 0;
	unsigned int intr_disabled = 0;

	/* We better have disabled before calling enable */
	assert(cos_nesting);
	cos_nesting--;

	/* Actually enable interrupts */
	if(!cos_nesting) {
		do {
			unsigned int rk_disabled;
			tmp = cos_isr;

			isr_get(tmp, &rk_disabled, &intr_disabled, &contending);

			/* no more cos_nesting, actually "reenable" interrupts */
			final = isr_construct(0, intr_disabled, contending);

		} while (unlikely(!ps_cas((unsigned long *)&cos_isr, tmp, final)));
	}

	isintr = intr_disabled;
	return contending;
}

static inline void
intr_disable(void)
{ isr_disable(); }

static inline void
intr_enable(void)
{
	unsigned int contending;
        tcap_prio_t prio;
	int ret;

	contending = isr_enable();
	if (unlikely(contending && !isintr)) {
		/*
		 * Assumption here: we have actually re-enabled
		 * interrupts here as opposed to release another
		 * cos_nesting of the interrupts
		 */
		do {
			//ret = cos_switch(irq_thdcap[contending], intr_eligible_tcap(contending), irq_prio[contending],
			//		 TCAP_TIME_NIL, BOOT_CAPTBL_SELF_INITRCV_BASE, cos_sched_sync());
			ret = cos_thd_switch(irq_thdcap[contending]);
			assert (ret == 0 || ret == -EAGAIN);
		} while(ret == -EAGAIN);
	}
}

#endif

#ifndef COS_SYNC_H
#define COS_SYNC_H

#include <assert.h>
#include <cos_types.h>
#include <cos_kernel_api.h>

typedef u32_t isr_state_t;
extern volatile isr_state_t cos_isr; /* Last running isr thread */
extern unsigned int cos_nesting;     /* Depth to intr_disable/intr_enable */
extern u32_t intrs; 	             /* Intrrupt bit mask */

int __attribute__((format(printf,1,2))) printc(char *fmt, ...);

int  intr_getdisabled(int intr);
void intr_start(thdcap_t tid);
void intr_end(void);
void event_process(int pending, int tid, int rcving);

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


static inline void
isr_get(isr_state_t tmp, unsigned int *isdisabled, thdcap_t *contending)
{
	*isdisabled = tmp >> 31;
	/* 
	 * 1. Get the first 16 bits
	 * 2. Zero out top bit so disabled / enabled interrupts does not affect contended value
	 */
	*contending = (thdcap_t)(tmp & ((~0) >> 16) & !(1<<31));
}

static inline isr_state_t
isr_construct(unsigned int isdisabled, thdcap_t contending)
{
	assert(isdisabled <= 1);
	return (u32_t)isdisabled << 31 | (u32_t)contending;
	//assert(isdisabled <= 1 && (unsigned int)cos_nesting < (1<<16));
	//return (u32_t)isdisabled << 31 | (u32_t)cos_nesting << 16 | (u32_t)contending;
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
			 * A.K.A assert(!cos_isr)
			 */
			assert(!isdisabled);
			assert(!cos_nesting);
			assert(!contending);
			//assert(!(!isdisabled && contending)); /* TODO: validate this assertion */
			final = isr_construct(1, contending);
		} while (unlikely(!ps_cas(&cos_isr, tmp, final)));
	}
		
	cos_nesting++;
	assert((unsigned int)cos_nesting < (1<<16));
}

static inline thdcap_t
isr_enable(void)
{
	isr_state_t tmp, final;
	thdcap_t contending = 0;

	/* We better have disabled b4 calling enable */	
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

		} while (unlikely(!ps_cas(&cos_isr, tmp, final)));
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
	int ret;

	/* FIXME: add logic for synchronization with the kernel */
	contending = isr_enable();
	if (unlikely(contending)) {
		/*
		 * Assumption here: we have actually re-enabled
		 * interrupts here as opposed to release another
		 * cos_nesting of the interrupts
		 */
		do {

			ret = cos_switch(contending, 0, 0, 0, BOOT_CAPTBL_SELF_INITRCV_BASE, cos_sched_sync());
			assert (ret == 0 || ret == -EAGAIN);
		} while (ret == -EAGAIN);
	}
}

#endif

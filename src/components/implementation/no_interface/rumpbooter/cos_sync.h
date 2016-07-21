#ifndef COS_SYNC_H
#define COS_SYNC_H

#include <assert.h>
#include <cos_types.h>

typedef u32_t isr_state_t;
volatile isr_state_t cos_isr = 0; /* Last running isr thread */
unsigned int nesting = 0; 	  /* Depth to intr_disable/intr_enable */

int __attribute__((format(printf,1,2))) printc(char *fmt, ...);

int  intr_getdisabled(int intr);
void intr_start(thdcap_t tid);
void intr_end(void);
void event_process(int pending, int tid, int rcving);

static inline void
isr_get(isr_state_t tmp, unsigned int *isdisabled, thdcap_t *contending)
{
	*isdisabled = tmp >> 31;
	*contending = (thdcap_t)(tmp & ((~0) >> 16));
}

static inline isr_state_t
isr_construct(unsigned int isdisabled, thdcap_t contending)
{
	assert(isdisabled <= 1);
	return (u32_t)isdisabled << 31 | (u32_t)contending;
	//assert(isdisabled <= 1 && (unsigned int)nesting < (1<<16));
	//return (u32_t)isdisabled << 31 | (u32_t)nesting << 16 | (u32_t)contending;
}

/* Set highest order bit in cos_isr to 1 or increment nesting count and return */
static inline void
isr_disable(void)
{
	isr_state_t tmp, final;

	/* Isr is currently enabled, disable for first time */
	if(!nesting) {
		do {
			unsigned int isdisabled;
			thdcap_t contending;

			tmp = cos_isr;
			isr_get(tmp, &isdisabled, &contending);	
			/*
			 * Intrrupts must be enabled and nesting must be 0
			 * Cannot have enabled and contentended.  This would imply
			 * that a non-interrupt thread has preempted an interrupt
			 * thread.
			 * A.K.A assert(!cos_isr)
			 */
			assert(!isdisabled && !nesting && !contending);
			//assert(!(!isdisabled && contending)); /* TODO: validate this assertion */
			final = isr_construct(1, contending);
		} while (unlikely(!ps_cas(&cos_isr, tmp, final)));
	}
		
	nesting++;
	assert((unsigned int)nesting < (1<<16));
}

static inline thdcap_t
isr_enable(void)
{
	isr_state_t tmp, final;
	thdcap_t contending = 0;

	/* We better have disabled b4 calling enable */
	assert(nesting);
	nesting--;

	/* Actually enable interrupts */
	if(!nesting) {
		do {
			unsigned int isdisabled;

			tmp = cos_isr;
			isr_get(tmp, &isdisabled, &contending);
			assert(isdisabled);

			/* no more nesting, actually "reenable" interrupts */
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
		 * nesting of the interrupts
		 */
		ret = cos_switch(contending, 0, 0, 0, BOOT_CAPTBL_SELF_INITRCV_BASE);
		if (ret && ret != -EAGAIN) {
			printc("intr_enable, FAILED cos_switch %s\n", strerror(ret));
			assert(0);
		}
	}
}

#endif

#ifndef UTIL_H
#define UTIL_H

#define CAS_SUCCESS 1

#define CAV7_SFR(base, offset) (*((volatile unsigned long *)((unsigned long)((base) + (offset)))))

/* Basic assembly for Cortex-A */
static inline unsigned long
cos_ldrexw(volatile unsigned long *addr)
{
	unsigned long result;
	__asm__ __volatile__("ldrex %0, %1" : "=r"(result) : "Q"(*addr));
	return (result);
}

static inline unsigned long
cos_strexw(unsigned long value, volatile unsigned long *addr)
{
	unsigned long result;
	__asm__ __volatile__("strex %0, %2, %1" : "=&r"(result), "=Q"(*addr) : "r"(value));
	return (result);
}


static inline void
cos_clrex(void)
{
	__asm__ __volatile__("clrex" ::: "memory");
}

/*
 * Return values:
 * 0 on failure due to contention (*target != old)
 * 1 otherwise (*target == old -> *target = updated)
 */
static inline int
cos_cas(unsigned long *target, unsigned long old, unsigned long updated)
{
	unsigned long oldval, res;

	do {
		oldval = cos_ldrexw(target);

		if (oldval == old) /* 0=succeeded, 1=failed */
			res = cos_strexw(updated, target);
		else {
			cos_clrex();
			return 0;
		}
	} while (res);

	return 1;
}

/*
 * Fetch-and-add implementation on Cortex-A. Returns the original value.
 */
static inline int
cos_faa(int *var, int value)
{
	unsigned int res;
	int          oldval;

	do {
		oldval = (int)cos_ldrexw((volatile unsigned long *)var);
		res    = cos_strexw((unsigned long)(oldval + value), (volatile unsigned long *)var);
	} while (res);

	return oldval;
}

/* Cortex-A isb instruction barrier. */
static inline void
cos_inst_bar(void)
{
	__asm__ __volatile__("isb" :::);
}

#ifndef rdtscll
static inline unsigned long long
__rdtscll(void)
{
/* Global timer base */
#define CAV7_GTMR_BASE 0xF8F00200
/* Global timer registers */
#define CAV7_GTMR_GTCNTRL CAV7_SFR(CAV7_GTMR_BASE, 0x0000)
#define CAV7_GTMR_GTCNTRH CAV7_SFR(CAV7_GTMR_BASE, 0x0004)
#define CAV7_GTMR_GTCTLR CAV7_SFR(CAV7_GTMR_BASE, 0x0008)

	unsigned long      tsc_h_first;
	unsigned long      tsc_h_second;
	unsigned long      tsc_l;
	unsigned long long tsc;

	do {
		tsc_h_first  = CAV7_GTMR_GTCNTRH;
		tsc_l        = CAV7_GTMR_GTCNTRL;
		tsc_h_second = CAV7_GTMR_GTCNTRH;
	} while (tsc_h_first != tsc_h_second);

	tsc = tsc_h_first;
	tsc <<= 32;
	tsc += tsc_l;

	return tsc << 1;
}
#define rdtscll(val)               \
	do {                       \
		val = __rdtscll(); \
	} while (0)

#endif

#endif

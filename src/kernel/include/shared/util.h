#ifndef UTIL_H
#define UTIL_H

#define CAS_SUCCESS 1

/* 
 * Return values:
 * 0 on failure due to contention (*target != old)
 * 1 otherwise (*target == old -> *target = updated)
 */
static inline int 
cos_dcas(unsigned long long *target, unsigned long long old, unsigned long long updated)
{
	char z;
	unsigned int old_h, old_l, new_h, new_l;
	old_h = (unsigned int)(old >> 32);
	old_l = (unsigned int)(old & 0xffffffff);
	new_h = (unsigned int)(updated >> 32);
	new_l = (unsigned int)(updated & 0xffffffff);
	__asm__ __volatile__("lock CMPXCHG8B %0; setz %1"
			     : "+m" (*target),
			       "=a" (z)
			     : "d"  (old_h),
			       "a"  (old_l),
			       "c"  (new_h),
			       "b"  (new_l)
			     : "memory", "cc");
	return (int)z;
}

/* Fetch-and-add implementation on x86. It returns the original value
 * before xaddl. */
static inline int 
cos_faa(int *var, int value)
{
	asm volatile("lock xaddl %%eax, %2;"
		     :"=a" (value)            //Output
		     :"a" (value), "m" (*var) //Input
		     :"memory");
	return value;
}

/* x86 cpuid instruction barrier. */
static inline void
cos_inst_bar(void)
{
	int eax, edx, code = 0;
	
	asm volatile("cpuid"
		     :"=a"(eax),"=d"(edx)
		     :"a"(code)
		     :"ecx","ebx");

	return;
}

static inline void cos_mem_fence(void)
{
	__asm__ __volatile__("mfence" ::: "memory");
}

#ifndef rdtscll
#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))
#endif

#endif


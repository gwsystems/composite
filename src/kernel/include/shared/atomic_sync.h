#ifndef UTIL_H
#define UTIL_H

#define CAS_SUCCESS 1

/*
 * Return values:
 * 0 on failure due to contention (*target != old)
 * 1 otherwise (*target == old -> *target = updated)
 */
static inline int
cos_cas(unsigned long *target, unsigned long cmp, unsigned long updated)
{
	char z;
	__asm__ __volatile__("lock cmpxchgl %2, %0; setz %1"
	                     : "+m"(*target), "=a"(z)
	                     : "q"(updated), "a"(cmp)
	                     : "memory", "cc");
	return (int)z;
}

/*
 * Return values:
 * 0 on failure due to contention (*target != old)
 * 1 otherwise (*target == old -> *target = updated)
 */
static inline int
cos_dcas(unsigned long long *target, unsigned long long old, unsigned long long updated)
{
	char         z;
	unsigned int old_h, old_l, new_h, new_l;
	old_h = (unsigned int)(old >> 32);
	old_l = (unsigned int)(old & 0xffffffff);
	new_h = (unsigned int)(updated >> 32);
	new_l = (unsigned int)(updated & 0xffffffff);
	__asm__ __volatile__("lock CMPXCHG8B %0; setz %1"
	                     : "+m"(*target), "=a"(z)
	                     : "d"(old_h), "a"(old_l), "c"(new_h), "b"(new_l)
	                     : "memory", "cc");
	return (int)z;
}

/* Fetch-and-add implementation on x86. It returns the original value
 * before xaddl. */
static inline int
cos_faa(int *var, int value)
{
	asm volatile("lock xaddl %%eax, %2;"
	             : "=a"(value)           // Output
	             : "a"(value), "m"(*var) // Input
	             : "memory");
	return value;
}

/* CK implements cas and faa with all types(byte, word, etc) */
/* Byte width fetch-and-add implementation on x86. It returns the
 * original value before xaddl. */
static inline int
cos_faa_byte(u8_t *var, u8_t value)
{
	asm volatile("lock xaddb %0, %1;"
	             : "+q"(value) // Output
	             : "m"(*var)   // Input
	             : "memory");
	return value;
}

/* Atomic_or implementation on x86. */
static inline void
cos_atomic_or(int *var, int value)
{
	asm volatile("lock orl %%eax, %1;"
	             :                       // Output
	             : "a"(value), "m"(*var) // Input
	             : "memory");
}

/* Atomic_and implementation on x86. */
static inline void
cos_atomic_and(int *var, int value)
{
	asm volatile("lock andl %%eax, %1;"
	             :                       // Output
	             : "a"(value), "m"(*var) // Input
	             : "memory");
}

/* x86 cpuid instruction barrier. */
static inline void
cos_inst_bar(void)
{
	int eax, edx, code = 0;

	asm volatile("cpuid" : "=a"(eax), "=d"(edx) : "a"(code) : "ecx", "ebx");

	return;
}

static inline void
cos_mem_fence(void)
{
	__asm__ __volatile__("mfence" ::: "memory");
}

static inline void
cos_compiler_barrier(void)
{
	asm volatile("" ::: "memory");
}

#define COS_LOAD(x) (*(volatile typeof(x) *)&(x))

#ifndef rdtscll
#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A"(val))
#endif

#endif

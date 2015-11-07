#ifndef PS_PLAT_LINUX_H
#define PS_PLAT_LINUX_H

#include <assert.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>

#define u16_t unsigned short int
#define u32_t unsigned int
#define u64_t unsigned long long
typedef u64_t ps_tsc_t; 	/* our time-stamp counter representation */
typedef u16_t coreid_t;

/* Default allocation and deallocation functions */
static inline void *
ps_plat_alloc(size_t sz, coreid_t coreid)
{ 
	(void)coreid; 
	return mmap(0, sz, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, (size_t)0); 
}

static inline void
ps_plat_free(void *s, size_t sz, coreid_t coreid)
{ 
	(void)coreid; 
	munmap(s, sz); 
}

#define PS_CACHE_LINE  64
#define PS_CACHE_PAD   (PS_CACHE_LINE*2)
#define PS_WORD        sizeof(long)
#define PS_PACKED      __attribute__((packed))
#define PS_ALIGNED     __attribute__((aligned(PS_CACHE_LINE)))
#define PS_WORDALIGNED __attribute__((aligned(PS_WORD)))
#ifndef PS_NUMCORES
#define PS_NUMCORES    10
#endif
#define PS_PAGE_SIZE   4096
#define PS_RNDUP(v, a) (-(-(v) & -(a))) /* from blogs.oracle.com/jwadams/entry/macros_and_powers_of_two */

/* 
 * We'd like an expression here so that it can be used statically, and
 * the compiler will turn it into a constant.
 * 
 * Value "v" must be an unsigned type the size of a word (e.g. unsigned long).
 * 
 * from http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2Float 
 */
static inline unsigned long
ps_rndpow2(unsigned long v)
{
	v--;
	v |= v >> 1;
	v |= v >> 2;
	v |= v >> 4;
	v |= v >> 8;
	v |= v >> 16;
	if (sizeof(long) == 8) v |= v >> 32; /* 64 bit systems */
	v++;

	return v;
}

#define EQUIESCENCE (200)

#ifndef likely
#define likely(x)      __builtin_expect(!!(x), 1)
#endif
#ifndef unlikely
#define unlikely(x)    __builtin_expect(!!(x), 0)
#endif

static inline ps_tsc_t
ps_tsc(void)
{
	unsigned long a, d, c;

	__asm__ __volatile__("rdtsc" : "=a" (a), "=d" (d), "=c" (c) : : );

	return ((u64_t)d << 32) | (u64_t)a;
}

static inline ps_tsc_t
ps_tsc_locality(coreid_t *coreid, coreid_t *numaid)
{
	unsigned long a, d, c;

	__asm__ __volatile__("rdtscp" : "=a" (a), "=d" (d), "=c" (c) : : );
	*coreid = c & 0xFFF; 	/* lower 12 bits in Linux = coreid */
	*numaid = c >> 12; 	/* next 8 = socket/numa id */

	return ((u64_t)d << 32) | (u64_t)a;
}

static inline unsigned int
ps_coreid(void)
{
	coreid_t coreid, numaid;

	if (PS_NUMCORES == 1) return 0;
	ps_tsc_locality(&coreid, &numaid);

	return coreid;
}

#ifndef ps_cc_barrier
#define ps_cc_barrier() __asm__ __volatile__ ("" : : : "memory")
#endif

#define PS_ATOMIC_POSTFIX "q" /* x86-64 */
/* #define PS_ATOMIC_POSTFIX "l" */ /* x86-32 */
#define PS_CAS_INSTRUCTION "cmpxchg"
#define PS_FAA_INSTRUCTION "xadd"
#define PS_CAS_STR PS_CAS_INSTRUCTION PS_ATOMIC_POSTFIX " %2, %0; setz %1"
#define PS_FAA_STR PS_FAA_INSTRUCTION PS_ATOMIC_POSTFIX " %1, %0"

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

static inline long
ps_faa(unsigned long *target, long inc)
{
        __asm__ __volatile__("lock " PS_FAA_STR
                             : "+m" (*target), "+q" (inc)
                             : : "memory", "cc");
        return inc;
}

/*
 * Only atomic on a uni-processor, so not for cross-core coordination.
 * Faster on a multiprocessor when used to synchronize between threads
 * on a single core by avoiding locking.
 */
static inline int
ps_upcas(unsigned long *target, unsigned long old, unsigned long updated)
{
        char z;
        __asm__ __volatile__(PS_CAS_STR
                             : "+m" (*target), "=a" (z)
                             : "q"  (updated), "a"  (old)
                             : "memory", "cc");
        return (int)z;
}

static inline void
ps_mem_fence(void)
{ __asm__ __volatile__("mfence" ::: "memory"); }


/*
 * FIXME: this is truly an affront to humanity for now, but it is a
 * simple lock for testing -- naive spin *without* backoff, gulp
 */
struct ps_lock {
	unsigned long o;
};

static inline void
ps_lock_take(struct ps_lock *l)
{ while (!ps_cas(&l->o, 0, 1)) ; }

static inline void
ps_lock_release(struct ps_lock *l)
{ l->o = 0; }

static inline void
ps_lock_init(struct ps_lock *l)
{ ps_lock_release(l); }

#endif	/* PS_PLAT_LINUX_H */

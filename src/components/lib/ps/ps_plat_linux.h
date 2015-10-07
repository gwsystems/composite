#ifndef PS_PLAT_LINUX_H
#define PS_PLAT_LINUX_H

#include <assert.h>
#include <sys/mman.h>
#include <stdio.h>

static inline void *
ps_plat_alloc(size_t sz) {
	/* printf("mmap(%d)\n", sz); */
	return mmap(0, sz, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, (size_t)0);
}

#define PS_SLAB_ALLOC(sz) ps_plat_alloc(sz)
#define PS_SLAB_FREE(x, sz) do { /* printf("munmap(%d)\n", sz); */ munmap(x, sz); } while(0)
#define u16_t unsigned short int
#define u32_t unsigned int
#define u64_t unsigned long long
#define unlikely(x) x

#define PS_CACHE_LINE  64
#define PS_WORD        sizeof(long)
#define PS_PACKED      __attribute__((packed))
#define PS_ALIGNED     __attribute__((aligned(PS_CACHE_LINE)))
#define PS_WORDALIGNED __attribute__((aligned(PS_WORD)))
#define PS_NUMCORES    1
#define PS_PAGE_SIZE   4096
#define PS_RNDUP(v, a) (-(-(v) & -(a))) /* from blogs.oracle.com/jwadams/entry/macros_and_powers_of_two */


static inline
u64_t ps_tsc(void)
{
	unsigned long a, d, c;

	__asm__ __volatile__("rdtsc" : "=a" (a), "=d" (d), "=c" (c) : : );

	return ((u64_t)d << 32) | (u64_t)a;
}

static inline
u64_t ps_tsc_locality(unsigned int *coreid, unsigned int *numaid)
{
	unsigned long a, d, c;

	__asm__ __volatile__("rdtscp" : "=a" (a), "=d" (d), "=c" (c) : : );
	*coreid = c & 0xFFF; 	/* lower 12 bits in Linux = coreid */
	*numaid = c >> 12; 	/* next 8 = socket/numa id */

	return ((u64_t)d << 32) | (u64_t)a;
}

static inline void
ps_tsc_read(u64_t *tsc)
{ *tsc = ps_tsc(); }

static inline unsigned int
ps_coreid(void)
{
	return 0; 		/*  testing... */
	/* unsigned int coreid, numaid; */

	/* ps_rdtsc_locality(&coreid, &numaid); */

	/* return coreid; */
}

#endif	/* PS_PLAT_LINUX_H */

#ifndef GET_TIME_H
#define GET_TIME_H

#include <time.h>
#include <sys/time.h>

#ifndef WASM
#ifndef CPU_FREQ
#define CPU_FREQ 1000
#endif
#endif

static unsigned long long
get_time()
{
#if 0
  unsigned long long int ret = 0;
  unsigned int cycles_lo;
  unsigned int cycles_hi;
  __asm__ volatile ("RDTSC" : "=a" (cycles_lo), "=d" (cycles_hi));
  ret = (unsigned long long int)cycles_hi << 32 | cycles_lo;

  return ret;
#else
    struct timeval Tp;
    int stat;
    stat = gettimeofday (&Tp, NULL);
    if (stat != 0)
      printf ("Error return from gettimeofday: %d", stat);
    return (Tp.tv_sec * 1000000 + Tp.tv_usec);
#endif
}

static inline void
print_time(unsigned long long s, unsigned long long e)
{
#if 0
	printf("%llu cycs, %llu us\n", e - s, (e - s) / CPU_FREQ);
#else
	fprintf(stderr, "%llu us\n", e - s);
#endif
}

#endif /* GET_TIME_H */

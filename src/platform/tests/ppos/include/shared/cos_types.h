#ifndef COS_TYPES_T
#define COS_TYPES_T

#define LINUX_TEST

#ifndef unlikely
#define unlikely(x)     __builtin_expect(!!(x), 0)
#endif
#ifndef likely
#define likely(x)     __builtin_expect(!!(x), 1)
#endif

typedef unsigned char      u8_t;
typedef unsigned short int u16_t;
typedef unsigned int       u32_t;
typedef unsigned long long u64_t;
typedef signed char      s8_t;
typedef signed short int s16_t;
typedef signed int       s32_t;
typedef signed long long s64_t;

typedef u16_t thdid_t;

typedef unsigned long vaddr_t;
typedef unsigned long paddr_t;

#define PAVAOFF ((1<<30) * 3)
static void *chal_va2pa(void *va) { return (void *)((u32_t)va - PAVAOFF); }
static void *chal_pa2va(void *pa) { return (void *)((u32_t)pa + PAVAOFF); }

#define HALF_CACHE_ALIGNED __attribute__((aligned(32)))
#define CACHE_ALIGNED __attribute__((aligned(64)))
#define PAGE_ALIGNED __attribute__((aligned(4096)))

struct pt_regs {
	unsigned long ax, bx, cx, dx, bp, sp, ip;
};

#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

#endif

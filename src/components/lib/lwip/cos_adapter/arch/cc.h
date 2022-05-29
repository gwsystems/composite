#ifndef __ARCH_CC_H__
#define __ARCH_CC_H__

#define LWIP_NO_STDINT_H 1

#ifndef COS_BASE_TYPES
#define COS_BASE_TYPES
typedef unsigned char      u8_t;
typedef unsigned short int u16_t;
typedef unsigned int       u32_t;
typedef unsigned long long u64_t;
typedef signed char        s8_t;
typedef signed short int   s16_t;
typedef signed int         s32_t;
typedef signed long long   s64_t;
#endif

typedef unsigned long int mem_ptr_t;

/* Define platform endianness */
#ifndef BYTE_ORDER
#define BYTE_ORDER LITTLE_ENDIAN
#endif /* BYTE_ORDER */

/* Compiler hints for packing structures */
// #define PACK_STRUCT_FIELD(x) x __attribute__((packed))
#define PACK_STRUCT_STRUCT __attribute__((packed))
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END

#include <llprint.h>
#define LWIP_PLATFORM_DIAG(x) do { printc x; } while (0);
#define LWIP_PLATFORM_ASSERT(x) do { printc(x); } while (0);
#endif /* __ARCH_CC_H__ */

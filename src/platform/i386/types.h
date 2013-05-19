#ifndef _TYPES_H_
#define _TYPES_H_

/* Usual types, this needs to be cleaned up a bit */

#define NULL ((void *)0x0)

typedef unsigned long uintptr_t;
typedef unsigned long size_t;

typedef signed int int32_t;
typedef unsigned int uint32_t;

typedef signed short int16_t;
typedef unsigned short uint16_t;

typedef signed char int8_t;
typedef unsigned char uint8_t;

typedef long long int64_t;
typedef unsigned long long uint64_t;

typedef long clock_t;

typedef int timer_t;

#endif

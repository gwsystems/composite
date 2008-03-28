/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef CONSTS_H
#define CONSTS_H

#ifdef __KERNEL__
#include <linux/thread_info.h> /* for PAGE_SIZE */
#else 
#define PAGE_SIZE (1<<12)
struct pt_regs { int dummy[16]; };
#endif

#define MAX_SERVICE_DEPTH 31
#define MAX_NUM_THREADS 7
#define MAX_SCHED_HIER_DEPTH 4

#define PAGE_MASK (~(PAGE_SIZE-1))
#define PGD_RANGE (1<<22)
#define PGD_MASK  (~(PGD_RANGE-1))
#define PGD_PER_PTBL 1024

#define round_to_page(x) ((x)&PAGE_MASK)
#define round_up_to_page(x) (((x)+PAGE_SIZE-1)&PAGE_MASK)
#define round_to_pgd_page(x) ((x)&PGD_MASK)
#define round_up_to_pgd_page(x) (((x)+PGD_RANGE-1)&PGD_MASK)

#define CACHE_LINE (32)
#define CACHE_ALIGNED __attribute__ ((aligned (CACHE_LINE)))
#define HALF_CACHE_ALIGNED __attribute__ ((aligned (CACHE_LINE/2)))
#define PAGE_ALIGNED __attribute__ ((aligned(PAGE_SIZE)))

#define SHARED_REGION_START (1<<30)  // 1 gig
#define SHARED_REGION_SIZE PGD_RANGE
#define SERVICE_START (SHARED_REGION_START+SHARED_REGION_SIZE)
/* size of virtual address spanned by one pgd entry */
#define SERVICE_SIZE PGD_RANGE
#define COS_INFO_REGION_ADDR SHARED_REGION_START
#define COS_DATA_REGION_LOWER_ADDR (COS_INFO_REGION_ADDR+PAGE_SIZE)
#define COS_DATA_REGION_MAX_SIZE (MAX_NUM_THREADS*PAGE_SIZE)

#define COS_NUM_ATOMIC_SECTIONS 10

#define COS_MAX_MEMORY 2048

#endif

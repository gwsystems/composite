/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

/* 
 * This file is included by both the kernel and by components.  Thus
 * any defines might need to be, unfortunately, made using ifdefs
 */

#ifndef CONSTS_H
#define CONSTS_H

#ifndef __ASM__
#ifdef __KERNEL__
#include <linux/thread_info.h> /* for PAGE_SIZE */
#else 
struct pt_regs {
        long ebx;
        long ecx;
        long edx;
        long esi;
        long edi;
        long ebp;
        long eax;
        int  xds;
        int  xes;
        int  xfs;
        /* int  gs; */
        long orig_eax;
        long eip;
        int  xcs;
        long eflags;
        long esp;
        int  xss;
};
//struct pt_regs { int dummy[16]; };
#endif
#endif
#ifndef __KERNEL__
#define PAGE_SIZE (1<<12)
#endif

#define MAX_SERVICE_DEPTH 31
#define MAX_NUM_THREADS 64
/* Stacks are 2 * page_size (expressed in words) */
#define MAX_STACK_SZ    (PAGE_SIZE*2/4)
#define ALL_STACK_SZ    (MAX_NUM_THREADS*MAX_STACK_SZ)
#define MAX_SCHED_HIER_DEPTH 4

#define MAX_NUM_SPDS 64
#define MAX_STATIC_CAP 1024

#define PAGE_MASK (~(PAGE_SIZE-1))
#define PGD_RANGE (1<<22)
#define PGD_MASK  (~(PGD_RANGE-1))
#define PGD_PER_PTBL 1024

#define round_to_page(x) (((unsigned long)x)&PAGE_MASK)
#define round_up_to_page(x) (round_to_page(x)+PAGE_SIZE)
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

#include "../asm_ipc_defs.h"

#endif

/**
 * Copyright 2014 by Qi Wang, interwq@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 */

#ifndef RETYPE_TBL_H
#define RETYPE_TBL_H

#include "shared/cos_types.h"
#include "shared/cos_config.h"
#include "shared/util.h"
#include "chal/cpuid.h"
#include "chal.h"

typedef enum {
	RETYPETBL_UNTYPED  = 0, /* untyped physical frames. */
	RETYPETBL_USER     = 1,
	RETYPETBL_KERN     = 2,
	RETYPETBL_RETYPING = 3, /* ongoing retyping operation */
} mem_type_t;

#define RETYPE_ENT_TYPE_SZ 4
#define RETYPE_ENT_REFCNT_SZ (sizeof(u32_t) * 8 - RETYPE_ENT_TYPE_SZ)
#define RETYPE_REFCNT_MAX ((1 << RETYPE_ENT_REFCNT_SZ) - 1)

/* This maintains the information of a collection of physical pages
 * (on one core). */
struct retype_entry {
	union refcnt_atom {
		struct {
			mem_type_t type : RETYPE_ENT_TYPE_SZ;
			/* The ref_cnt is the # of mappings in user space for user
			 * memory. For kernel memory, it's the # of pages that have
			 * been activated as kernel objects. */
			int ref_cnt : RETYPE_ENT_REFCNT_SZ;
		} __attribute__((packed));
		u32_t v;
	} refcnt_atom;
	u32_t __pad; /* let's make the size power of 2. */
	/* the timestamp of when the last unmapped happened. Used to
	 * track TLB quiescence when retype */
	u64_t last_unmap;
} __attribute__((packed));

/* When getting kernel memory through Linux, the retype table is
 * partitioned into two parts for kmem and user memory. */
/* # of mem_sets in total. +1 to be safe. */
#define N_USER_MEM_SETS (COS_MAX_MEMORY / RETYPE_MEM_NPAGES + 1)
#define N_KERN_MEM_SETS (COS_KERNEL_MEMORY / RETYPE_MEM_NPAGES + 1)

#define N_MEM_SETS (N_USER_MEM_SETS + N_KERN_MEM_SETS)
#define N_MEM_SETS_SZ (sizeof(struct retype_entry) * N_MEM_SETS)

/* per-cpu retype_info. Each core update this locally when doing
 * mapping/unmapping. */
struct retype_info {
	struct retype_entry mem_set[N_MEM_SETS];
	char                __pad[(N_MEM_SETS_SZ % CACHE_LINE == 0) ? 0 : (N_MEM_SETS_SZ % CACHE_LINE)];
} CACHE_ALIGNED;

extern struct retype_info retype_tbl[NUM_CPU];

/* global retype_info. We should only touch this when doing
 * retyping. NOT when doing mapping / unmapping. */
struct retype_info_glb {
	u32_t type;
	char  __pad[CACHE_LINE - sizeof(u32_t)];
} CACHE_ALIGNED;

extern struct retype_info_glb glb_retype_tbl[N_MEM_SETS];

#define COS_KMEM_BOUND (chal_kernel_mem_pa + PAGE_SIZE * COS_KERNEL_MEMORY)

/* physical address boundary check */
#define PA_BOUNDARY_CHECK()                                                                            \
	do {                                                                                           \
		if (unlikely(!(((u32_t)pa >= COS_MEM_START) && ((u32_t)pa < COS_MEM_BOUND))            \
		             && !(((u32_t)pa >= chal_kernel_mem_pa) && ((u32_t)pa < COS_KMEM_BOUND)))) \
			return -EINVAL;                                                                \
	} while (0)

/* get the index of the memory set. */
#define GET_MEM_IDX(pa)                                                                 \
	(((u32_t)pa >= COS_MEM_START) ? (((u32_t)(pa)-COS_MEM_START) / RETYPE_MEM_SIZE) \
	                              : (((u32_t)(pa)-chal_kernel_mem_pa) / RETYPE_MEM_SIZE + N_USER_MEM_SETS))
/* get the memory set struct of the current cpu */
#define GET_RETYPE_ENTRY(idx) ((&(retype_tbl[get_cpuid()].mem_set[idx])))
/* get the global memory set struct (used for retyping only). */
#define GET_GLB_RETYPE_ENTRY(idx) ((&(glb_retype_tbl[idx])))

#define cos_throw(label, errno) \
	{                       \
		ret = (errno);  \
		goto label;     \
	}

static inline int
retypetbl_cas(u32_t *a, u32_t old, u32_t new)
{
	return cos_cas((unsigned long *)a, old, new);
}

int retypetbl_retype2user(void *pa);
int retypetbl_retype2kern(void *pa);
int retypetbl_retype2frame(void *pa);

void retype_tbl_init(void);
int  retypetbl_ref(void *pa);
int  retypetbl_kern_ref(void *pa);
int  retypetbl_deref(void *pa);

#endif /* RETYPE_TBL_H */

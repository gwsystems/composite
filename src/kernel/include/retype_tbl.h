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
#include "cpuid.h"
#include "pgtbl.h"

#define RETYPE_MEM_NPAGES_SIZE   (RETYPE_MEM_NPAGES * PAGE_SIZE)

typedef enum {
	RETYPETBL_UNTYPED  = 0, /* untyped physical frames. */
	RETYPETBL_USER     = 1,
	RETYPETBL_KERN     = 2,
	RETYPETBL_RETYPING = 3, /* ongoing retyping operation */
} mem_type_t;

#define RETYPE_ENT_TYPE_SZ    4
#define RETYPE_ENT_REFCNT_SZ  (sizeof(u32_t)*8 - RETYPE_ENT_TYPE_SZ)
#define RETYPE_REFCNT_MAX     ((1 << RETYPE_ENT_REFCNT_SZ) - 1)

/* This maintains the information of a collection of physical pages
 * (on one core). */
struct retype_entry {
	union refcnt_atom {
		struct {
			mem_type_t type    : RETYPE_ENT_TYPE_SZ;
			/* The ref_cnt is the # of mappings in user space for user
			 * memory. For kernel memory, it's the # of pages that have
			 * been activated as kernel objects. */
			int        ref_cnt : RETYPE_ENT_REFCNT_SZ;
		} __attribute__((packed));
		u32_t v;
	} refcnt_atom;
	u32_t      __pad;   /* let's make the size power of 2. */
	/* the timestamp of when the last unmapped happened. Used to
	 * track TLB quiescence when retype */
	u64_t      last_unmap;
} __attribute__((packed));

/* When getting kernel memory through Linux, the retype table is
 * partitioned into two parts for kmem and user memory. */
/* # of mem_sets in total. +1 to be safe. */
#define N_USER_MEM_SETS (COS_MAX_MEMORY/RETYPE_MEM_NPAGES + 1)
#define N_KERN_MEM_SETS (COS_KERNEL_MEMORY/RETYPE_MEM_NPAGES + 1)

#define N_MEM_SETS    (N_USER_MEM_SETS + N_KERN_MEM_SETS)
#define N_MEM_SETS_SZ (sizeof(struct retype_entry) * N_MEM_SETS)

/* per-cpu retype_info. Each core update this locally when doing
 * mapping/unmapping. */
struct retype_info {
	struct retype_entry mem_set[N_MEM_SETS];
	char __pad[(N_MEM_SETS_SZ % CACHE_LINE == 0) ? 0 : (N_MEM_SETS_SZ % CACHE_LINE)];
} CACHE_ALIGNED;

extern struct retype_info retype_tbl[NUM_CPU];

/* global retype_info. We should only touch this when doing
 * retyping. NOT when doing mapping / unmapping. */
struct retype_info_glb {
	u32_t type;
	char __pad[CACHE_LINE - sizeof(u32_t)];
} CACHE_ALIGNED;

extern struct retype_info_glb glb_retype_tbl[N_MEM_SETS];

extern paddr_t kmem_start_pa;
#define COS_KMEM_BOUND (kmem_start_pa + PAGE_SIZE * COS_KERNEL_MEMORY)

/* physical address boundary check */
#define PA_BOUNDARY_CHECK() do { if (unlikely(!(((u32_t)pa >= COS_MEM_START) && ((u32_t)pa < COS_MEM_BOUND)) && \
					      !(((u32_t)pa >= kmem_start_pa) && ((u32_t)pa < COS_KMEM_BOUND)))) return -EINVAL; } while (0)

/* get the index of the memory set. */
#define GET_MEM_IDX(pa) (((u32_t)pa >= COS_MEM_START) ? (((u32_t)(pa) - COS_MEM_START) / RETYPE_MEM_NPAGES_SIZE) \
			 : (((u32_t)(pa) - kmem_start_pa) / RETYPE_MEM_NPAGES_SIZE + N_USER_MEM_SETS))
/* get the memory set struct of the current cpu */
#define GET_RETYPE_ENTRY(idx) ((&(retype_tbl[get_cpuid()].mem_set[idx])))
/* get the global memory set struct (used for retyping only). */
#define GET_GLB_RETYPE_ENTRY(idx) ((&(glb_retype_tbl[idx])))

#define cos_throw(label, errno) { ret = (errno); goto label; }

static inline int
retypetbl_cas(u32_t *a, u32_t old, u32_t new) {
	if (!cos_cas((unsigned long *)a, old, new)) return -ECASFAIL;

	return 0;
}

/* called to increment or decrement the refcnt. */
static inline int
mod_ref_cnt(void *pa, const int op, const mem_type_t type) 
{
	u32_t idx, old_v;
	struct retype_entry *retype_entry;
	union refcnt_atom local_u;

	PA_BOUNDARY_CHECK();

	idx = GET_MEM_IDX(pa);
	assert(idx < N_MEM_SETS);

	retype_entry = GET_RETYPE_ENTRY(idx);
	
	local_u.v = retype_entry->refcnt_atom.v;
	/* only allow to ref user or kernel typed memory set */
	if (unlikely(local_u.type != type)) return -EPERM;

	old_v = local_u.v;
	if (local_u.ref_cnt == RETYPE_REFCNT_MAX) return -EOVERFLOW;

	if (op) {
		local_u.ref_cnt = local_u.ref_cnt + 1;
	} else {
		local_u.ref_cnt = local_u.ref_cnt - 1;
		rdtscll(retype_entry->last_unmap);
	}
	cos_mem_fence();

	return retypetbl_cas(&(retype_entry->refcnt_atom.v), old_v, local_u.v);
}

static int
retypetbl_ref(void *pa, const mem_type_t type)
{
	return mod_ref_cnt(pa, 1, type);
}

static int
retypetbl_deref(void *pa, const mem_type_t type)
{
	return mod_ref_cnt(pa, 0, type);
}

static inline int
mod_mem_type(void *pa, const mem_type_t type)
{
	int i, ret;
	u32_t idx, old_type;
	struct retype_info_glb *glb_retype_info;

	PA_BOUNDARY_CHECK();

	idx = GET_MEM_IDX(pa);
	assert(idx < N_MEM_SETS);

	glb_retype_info = GET_GLB_RETYPE_ENTRY(idx);
	old_type = glb_retype_info->type;

	/* only can retype untyped mem sets. */
	if (unlikely(old_type != RETYPETBL_UNTYPED)) {
		if (old_type == type) return -EEXIST;
		else                  return -EPERM;
	}

	ret = retypetbl_cas(&(glb_retype_info->type), old_type, RETYPETBL_RETYPING);
	if (ret != CAS_SUCCESS) return ret;
	cos_mem_fence();

	/* Set the retyping flag successfully. Now nobody else can
	 * change this memory set. Update the per-core retype entries
	 * next. */
	for (i = 0; i < NUM_CPU; i++) {
		retype_tbl[i].mem_set[idx].refcnt_atom.type = type;
	}
	cos_mem_fence();

	/* Now commit the change to the global entry. */
	ret = retypetbl_cas(&(glb_retype_info->type), RETYPETBL_RETYPING, type);
	assert(ret == CAS_SUCCESS);

	return 0;
}

static int
retypetbl_retype2user(struct cap_pgtbl *cp, vaddr_t vaddr)
{
	void *pa;
	u32_t flags;
	unsigned long *pte;

	pte = pgtbl_lkup_pte(cp->pgtbl, vaddr, &flags);
	if (!pte) return -EINVAL;
	pa = (void *)(*pte & PGTBL_FRAME_MASK);

	return mod_mem_type(pa, RETYPETBL_USER);
}

static int
retypetbl_retype2kern(struct cap_pgtbl *cp, vaddr_t vaddr)
{
	void *pa;
	u32_t flags;
	unsigned long *pte;

	pte = pgtbl_lkup_pte(cp->pgtbl, vaddr, &flags);

	if (!pte) return -EINVAL;
	pa = (void *)(*pte & PGTBL_FRAME_MASK);

	return mod_mem_type(pa, RETYPETBL_KERN);
}

static int
retypetbl_retype2frame(struct cap_pgtbl *cp, vaddr_t vaddr)
{
	void *pa;
	u32_t flags;
	unsigned long *pte;

	struct retype_info_glb *glb_retype_info;
	union refcnt_atom local_u;
	u32_t old_v, idx, cpu;
	u64_t last_unmap;
	int ret, ref_sum;
	mem_type_t old_type;

	pte = pgtbl_lkup_pte(cp->pgtbl, vaddr, &flags);
	if (!pte) return -EINVAL;
	pa = (void *)(*pte & PGTBL_FRAME_MASK);

	PA_BOUNDARY_CHECK();

	idx = GET_MEM_IDX(pa);
	assert(idx < N_MEM_SETS);

	glb_retype_info = GET_GLB_RETYPE_ENTRY(idx);

	old_type = glb_retype_info->type;
	/* only can retype untyped mem sets. */
	if (unlikely(old_type != RETYPETBL_USER && old_type != RETYPETBL_KERN)) return -EPERM;

	/* lock down the memory set we are going to retype. */
	ret = retypetbl_cas(&(glb_retype_info->type), old_type, RETYPETBL_RETYPING);
	if (ret != CAS_SUCCESS) return ret;

	last_unmap = 0;
	for (ref_sum = 0, cpu = 0; cpu < NUM_CPU; cpu++) {
		/* Keep in mind that, ref_cnt on each core could be
		 * negative. */
		old_v = local_u.v = retype_tbl[cpu].mem_set[idx].refcnt_atom.v;
		assert(local_u.type == old_type || local_u.type == RETYPETBL_RETYPING);

		ref_sum += local_u.ref_cnt;
		local_u.type = RETYPETBL_RETYPING;

		ret = retypetbl_cas(&(retype_tbl[cpu].mem_set[idx].refcnt_atom.v), old_v, local_u.v);
		if (ret != CAS_SUCCESS) cos_throw(restore_all, -ECASFAIL);

		/* for tlb quiescence check */
		if (last_unmap < retype_tbl[cpu].mem_set[idx].last_unmap)
			last_unmap = retype_tbl[cpu].mem_set[idx].last_unmap;
	}

	/* only can retype when there's no more mapping */
	if (ref_sum != 0) cos_throw(restore_all, -ECASFAIL);

	/* before retype any type of memory back to untyped, we need
	 * to make sure TLB quiescence has been achieved after the
	 * last unmapping of any pages in this memory set. */
	if (!tlb_quiescence_check(last_unmap)) cos_throw(restore_all, -EQUIESCENCE);
	cos_mem_fence();

	/**************************/
	/**** Legit to retype! ****/
	/**************************/
	/* we already locked all entries. feel free to change here. */
	for (cpu = 0; cpu < NUM_CPU; cpu++) {
		retype_tbl[cpu].mem_set[idx].refcnt_atom.ref_cnt = 0;
		retype_tbl[cpu].mem_set[idx].refcnt_atom.type    = RETYPETBL_UNTYPED;
	}
	cos_mem_fence();

	/* and commit to the global entry. */
	ret = retypetbl_cas(&(glb_retype_info->type), RETYPETBL_RETYPING, RETYPETBL_UNTYPED);
	assert(ret == CAS_SUCCESS);

	return 0;
restore_all:
	/* Something went wrong: CAS, TLB quiescence or ref_sum
	 * non-zero. Clean-up here.*/

	/* restore per-core retype entries. (is this necessary?) */
	/* cpu is the one we tried to modify but fail. So we need to
	 * restore [0, cpu-1]. */
	for (cpu -= 1; cpu >= 0; cpu--) {
		old_v = local_u.v = retype_tbl[cpu].mem_set[idx].refcnt_atom.v;
		local_u.type = old_type;

		retypetbl_cas(&(retype_tbl[cpu].mem_set[idx].refcnt_atom.v), old_v, local_u.v);
	}
	/* and global entry. */
	{
		/* Restore the global entry to old type before return:
		 * only us can change it. */
		int ret_local = retypetbl_cas(&(glb_retype_info->type), RETYPETBL_RETYPING, old_type);
		assert(ret_local == CAS_SUCCESS);
	}

	return ret;
}

void retype_tbl_init(void);

#endif /* RETYPE_TBL_H */

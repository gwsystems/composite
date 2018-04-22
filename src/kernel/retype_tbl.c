/**
 * Copyright 2014 by Qi Wang, interwq@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 */

#include "include/retype_tbl.h"
#include "include/chal.h"
#include "include/shared/cos_types.h"
#include "include/shared/cos_errno.h"
#include "cc.h"
#include <assert.h>

/* The CPU-local data structures */
struct retype_info     retype_tbl[NUM_CPU] CACHE_ALIGNED;
struct retype_info_glb glb_retype_tbl[N_RETYPE_SLOTS] CACHE_ALIGNED;

/* The start positions of the tables - also architecture-specific */
const u32_t retype_slot_base[32] = {
  /*   0/1B    1/2B    2/4B    3/8B   4/16B   5/32B   6/64B  7/128B  8/256B  9/512B */
	-1,     -1,     -1,     -1,     -1,     -1,     -1,     -1,     -1,     -1,
  /*  10/1K   11/2K   12/4K   13/8K  14/16K  15/32K  16/64K 17/128K 18/256K 19/512K */
	-1,     -1,      0,     -1,     -1,     -1,     -1,     -1,     -1,     -1,
  /*  20/1M   21/2M   22/4M   23/8M  24/16M  25/32M  26/64M 27/128M 28/256M 29/512M */
	-1,     -1, N_MEM_SETS, -1,     -1,     -1,     -1,     -1,     -1,     -1,
  /*  30/1G   31/2G */
	-1,     -1
};

const u32_t walk_base[32] = {
  /*   0/1B    1/2B    2/4B    3/8B   4/16B   5/32B   6/64B  7/128B  8/256B  9/512B */
	-1,     -1,     -1,     -1,     -1,     -1,     -1,     -1,     -1,     -1,
  /*  10/1K   11/2K   12/4K   13/8K  14/16K  15/32K  16/64K 17/128K 18/256K 19/512K */
	-1,     -1,      0,     -1,     -1,     -1,     -1,     -1,     -1,     -1,
  /*  20/1M   21/2M   22/4M   23/8M  24/16M  25/32M  26/64M 27/128M 28/256M 29/512M */
	-1,     -1,      1,     -1,     -1,     -1,     -1,     -1,     -1,     -1,
  /*  30/1G   31/2G */
	-1,     -1
};

/**
 * Currently this is x86 specific. This array is used to find the previous page size's
 * order (the next larger superpage). A value of 0 means there are no such page, a value
 * of -1 means this page size is not supported on this architecture. This table starts at
 * 1B and ends at 2GB.
 */
const u32_t page_sizes_super[32] = {
  /*   0/1B    1/2B    2/4B    3/8B   4/16B   5/32B   6/64B  7/128B  8/256B  9/512B */
	-1,     -1,     -1,     -1,     -1,     -1,     -1,     -1,     -1,     -1,
  /*  10/1K   11/2K   12/4K   13/8K  14/16K  15/32K  16/64K 17/128K 18/256K 19/512K */
	-1,     -1,     22,     -1,     -1,     -1,     -1,     -1,     -1,     -1,
  /*  20/1M   21/2M   22/4M   23/8M  24/16M  25/32M  26/64M 27/128M 28/256M 29/512M */
	-1,     -1,      0,     -1,     -1,     -1,     -1,     -1,     -1,     -1,
  /*  30/1G   31/2G */
	-1,     -1
};

/* This array is to find the next smaller subpage */
const u32_t page_sizes_small[32] = {
  /*   0/1B    1/2B    2/4B    3/8B   4/16B   5/32B   6/64B  7/128B  8/256B  9/512B */
	-1,     -1,     -1,     -1,     -1,     -1,     -1,     -1,     -1,     -1,
  /*  10/1K   11/2K   12/4K   13/8K  14/16K  15/32K  16/64K 17/128K 18/256K 19/512K */
	-1,     -1,      0,     -1,     -1,     -1,     -1,     -1,     -1,     -1,
  /*  20/1M   21/2M   22/4M   23/8M  24/16M  25/32M  26/64M 27/128M 28/256M 29/512M */
	-1,     -1,     12,     -1,     -1,     -1,     -1,     -1,     -1,     -1,
  /*  30/1G   31/2G */
	-1,     -1
};

/* This only does reference for kernel typed memory, and we will not check the type */
int
retypetbl_kern_ref(void *pa, u32_t order)
{
	struct retype_entry_glb* p_glb;
	int ret, idx;

	assert(pa); /* cannot be NULL: kernel image takes that space */
	PA_BOUNDARY_CHECK();

	idx = GET_MEM_IDX(pa);
	assert(idx < N_MEM_SETS);

	p_glb = GET_GLB_RETYPE_ENTRY(idx, order);
	
	/* Atomic with FAA - necessary because of cluster counting */
	cos_faa((int*)&(p_glb->kernel_ref), 1);

	return 0;
}

int
retypetbl_kern_deref(void *pa, u32_t order)
{
	struct retype_entry_glb* p_glb;
	int ret, idx;

	assert(pa); /* cannot be NULL: kernel image takes that space */
	PA_BOUNDARY_CHECK();

	idx = GET_MEM_IDX(pa);
	assert(idx < N_MEM_SETS);

	p_glb = GET_GLB_RETYPE_ENTRY(idx, order);
	
	/* Atomic with FAA - necessary because of cluster counting */
	cos_faa((int*)&(p_glb->kernel_ref), -1);

	return 0;
}

/* Iteration macro for referencing - cannot use inline functions because we need i to rewind */
#define RETYPETBL_REF_ITERATE(TYPE_Y, TYPE_N) \
do { \
	for (i = MAX_PAGE_ORDER; i >= (int)order; i = page_sizes_small[i]) { \
		walk[walk_base[i]].p = GET_RETYPE_ENTRY(idx, i); \
		old_temp.refcnt_atom.v = walk[walk_base[i]].p->refcnt_atom.v; \
		temp.refcnt_atom.v = walk[walk_base[i]].p->refcnt_atom.v; \
		if (old_temp.refcnt_atom.type == (TYPE_N)) cos_throw(err, -EPERM); \
		if (old_temp.refcnt_atom.type == (TYPE_Y)) found = 1; \
		/* Increase or decrease the core-local count with CAS */ \
		temp.refcnt_atom.ref_cnt++; \
		if (retypetbl_cas(&(walk[walk_base[i]].p->refcnt_atom.v), old_temp.refcnt_atom.v, temp.refcnt_atom.v) != CAS_SUCCESS) cos_throw(err, -ECASFAIL); \
		walk[walk_base[i]].inc_cnt = 1; \
	} \
	if (!found) cos_throw(err, -EPERM); \
} \
while(0)

/* This will not return overflow anymore because the kernel/user counter is large enough */
int
retypetbl_ref(void *pa, u32_t order)
{
	/* FIXME:This can be very large on stack, consider shrinking it */
	struct page_record  walk[NUM_PAGE_SIZES];
	struct retype_entry temp, old_temp;
	int found = 0;
	int i, ret, idx;

	assert(pa); /* cannot be NULL: kernel image takes that space */
	PA_BOUNDARY_CHECK();

	idx = GET_MEM_IDX(pa);
	assert(idx < N_MEM_SETS);

	walk[walk_base[order]].p = GET_RETYPE_ENTRY(idx, order);
	if ((walk[walk_base[order]].p->refcnt_atom.type != RETYPETBL_USER) &&
	    (walk[walk_base[order]].p->refcnt_atom.type != RETYPETBL_KERN)) return -EPERM;

	walk[walk_base[MAX_PAGE_ORDER]].p = GET_RETYPE_ENTRY(idx, MAX_PAGE_ORDER);
	/* See if it is kernel or user */
	if(walk[walk_base[order]].p->refcnt_atom.type == RETYPETBL_USER) {
		RETYPETBL_REF_ITERATE(RETYPETBL_USER, RETYPETBL_KERN);
	} else {
		RETYPETBL_REF_ITERATE(RETYPETBL_KERN, RETYPETBL_USER);
	}
	
	return 0;
err:
	/* Prepare to unwind and quit */
	for (;i != 0; i = page_sizes_super[i]) {
		if (walk[walk_base[i]].inc_cnt != 0) walk[walk_base[i]].p->refcnt_atom.v--;
	}
	return ret;
}

/* Iteration macro for dereferencing - cannot use inline functions because we need i to rewind  */
#define RETYPETBL_DEREF_ITERATE(TYPE_Y, TYPE_N) \
do { \
	for (i = MAX_PAGE_ORDER; i >= (int)order; i = page_sizes_small[i]) { \
		walk[walk_base[i]].p = GET_RETYPE_ENTRY(idx, i); \
		if (walk[walk_base[i]].p->refcnt_atom.type == (TYPE_N)) cos_throw(err, -EPERM); \
		if (walk[walk_base[i]].p->refcnt_atom.type == (TYPE_Y)) found = 1; \
		walk[walk_base[i]].p->refcnt_atom.v--; \
		walk[walk_base[i]].inc_cnt = 1; \
	} \
	if (!found) cos_throw(err, -EPERM); \
} \
while(0)

int
retypetbl_deref(void *pa, u32_t order)
{
	/* FIXME:This can be very large on stack, consider shrinking it */
	struct page_record walk[NUM_PAGE_SIZES];
	int found = 0;
	int i, ret, idx;

	assert(pa); /* cannot be NULL: kernel image takes that space */
	PA_BOUNDARY_CHECK();

	idx = GET_MEM_IDX(pa);
	assert(idx < N_MEM_SETS);

	walk[walk_base[order]].p = GET_RETYPE_ENTRY(idx, order);
	if ((walk[walk_base[order]].p->refcnt_atom.type != RETYPETBL_USER) &&
            (walk[walk_base[order]].p->refcnt_atom.type != RETYPETBL_KERN)) return -EPERM;

	walk[walk_base[MAX_PAGE_ORDER]].p = GET_RETYPE_ENTRY(idx, MAX_PAGE_ORDER);
	/* See if it is kernel or user */
	if(walk[walk_base[order]].p->refcnt_atom.type == RETYPETBL_USER) {
		RETYPETBL_DEREF_ITERATE(RETYPETBL_USER, RETYPETBL_KERN);
	} else {
		RETYPETBL_DEREF_ITERATE(RETYPETBL_KERN, RETYPETBL_USER);
	}

	rdtscll(walk[walk_base[i]].p->last_unmap);
	cos_mem_fence();
	
	return 0;
err:
	/* Prepare to unwind and quit */
	for (;i != 0; i = page_sizes_super[i]) {
		if (walk[walk_base[i]].inc_cnt != 0) walk[walk_base[i]].p->refcnt_atom.v++;
	}
	return ret;
}

/* Iteration macro for modifying memory type - cannot use inline functions because we need i to rewind */
#define MOD_MEM_TYPE_ITERATE(REFCNT) \
do { \
	/* Repetitively add the record of a page set to a type, into the level one above it */ \
	for (i = page_sizes_small[MAX_PAGE_ORDER]; i >= (int)order; i = page_sizes_small[i]) { \
	        walk[walk_base[i]].p_glb = GET_GLB_RETYPE_ENTRY(idx, i); \
		walk[walk_base[i]].inc_cnt = 0; \
		/* Do we need to update the next highest page size count? */ \
		if (walk[walk_base[i]].p_glb->refcnt_atom.type == RETYPETBL_UNTYPED || walk[walk_base[i]].p_glb->refcnt_atom.REFCNT == 0) { \
			old_temp.refcnt_atom.v = walk[walk_base[page_sizes_super[i]]].p_glb->refcnt_atom.v; \
			if (old_temp.refcnt_atom.type != RETYPETBL_UNTYPED) cos_throw(err, -EPERM); \
			temp.refcnt_atom.v = old_temp.refcnt_atom.v; \
			temp.refcnt_atom.REFCNT++; \
			/*  \
			 * Use CAS to update the word here in case there is a retype/retype race. \
			 * If another retype retypes the parent, then the type check in this CAS \
			 * will fail. This CAS makes sure that the parent will not be retyped into \
			 * something else. \
			 */ \
			if (retypetbl_cas((u32_t*)(walk[walk_base[page_sizes_super[i]]].p_glb), old_temp.refcnt_atom.v, temp.refcnt_atom.v) != CAS_SUCCESS) cos_throw(err, -ECASFAIL); \
			/* This is a simple assignment, because this is the function's local variable */ \
			walk[walk_base[page_sizes_super[i]]].inc_cnt = 1; \
		} \
		if (walk[walk_base[i]].p_glb->refcnt_atom.type != RETYPETBL_UNTYPED) cos_throw(err, -ECASFAIL); \
	} \
} \
while(0)

/* Macro used to change types and clear counts atomically */
#define ATOMIC_TYPE_SWAP_AND_CLEAR(PTR, OLD_TYPE, NEW_TYPE) \
do { \
	old_temp.refcnt_atom.v = (PTR)->refcnt_atom.v; \
	old_temp.refcnt_atom.type = (OLD_TYPE); \
	temp.refcnt_atom.type = (NEW_TYPE); \
	temp.refcnt_atom.user_cnt = 0; \
	temp.refcnt_atom.kernel_cnt = 0; \
	ret = retypetbl_cas((u32_t*)(PTR), old_temp.refcnt_atom.v, temp.refcnt_atom.v); \
} \
while(0)

static inline int
mod_mem_type(void *pa, u32_t order, const mem_type_t type)
{
	struct page_record      walk[NUM_PAGE_SIZES];
	int                     i, ret;
	u32_t                   idx, old_type;
	struct retype_entry_glb temp;
	struct retype_entry_glb old_temp;
	struct retype_entry_glb *glb_retype_info;

	assert(pa); /* cannot be NULL: kernel image takes that space */
	PA_BOUNDARY_CHECK();

	idx = GET_MEM_IDX(pa);
	assert(idx < N_MEM_SETS);

	/* Kernel memory needs to be kernel accessible: pa2va returns null if it's not */
	if (type == RETYPETBL_KERN && chal_pa2va((paddr_t)pa) == NULL) return -EINVAL;

	/* Get the global slot */
        walk[walk_base[MAX_PAGE_ORDER]].p_glb = GET_GLB_RETYPE_ENTRY(idx, MAX_PAGE_ORDER);
	old_type = walk[walk_base[MAX_PAGE_ORDER]].p_glb->refcnt_atom.type;

	/* only can retype untyped mem sets. */
	if (unlikely(old_type != RETYPETBL_UNTYPED)) {
		if (old_type == type)
			return -EEXIST;
		else
			return -EPERM;
	}

	/* See if we are retyping this to kernel or user level */
	if (type == RETYPETBL_USER) {
		MOD_MEM_TYPE_ITERATE(user_cnt);
	} else {
		MOD_MEM_TYPE_ITERATE(kernel_cnt);
	}

	/* typed as the other type? - if there is a retype/retype race */
	if (((temp.refcnt_atom.type == RETYPETBL_KERN) && (type == RETYPETBL_USER)) ||
            ((temp.refcnt_atom.type == RETYPETBL_USER) && (type == RETYPETBL_KERN))) {
		cos_throw(err, -ECASFAIL);
	}

	/* atomic update with CAS */
	ATOMIC_TYPE_SWAP_AND_CLEAR(walk[walk_base[order]].p_glb, RETYPETBL_UNTYPED, RETYPETBL_RETYPING);
	if (ret != CAS_SUCCESS) cos_throw(err, -ECASFAIL);
	cos_mem_fence();

	/* 
	 * Set the retyping flag successfully. Now nobody else can
	 * change this memory set. Update the per-core retype entries
	 * next.
	 */
	for (i = 0; i < NUM_CPU; i++) {
		GET_RETYPE_CPU_ENTRY(i, idx, order)->refcnt_atom.type = type;
	}
	cos_mem_fence();

	/* Now commit the change to the global entry. */
	ATOMIC_TYPE_SWAP_AND_CLEAR(walk[walk_base[order]].p_glb, RETYPETBL_RETYPING, type);
	assert(ret == CAS_SUCCESS);

	return 0;
err:
	/* Prepare to unwind and quit */
	if (type == RETYPETBL_USER) {
		for (;i != 0; i = page_sizes_super[i]) {
			if (walk[walk_base[i]].inc_cnt != 0) cos_faa((int*)walk[walk_base[i]].p_glb, -(1 << RETYPE_ENT_GLB_REFCNT_SZ));
		}
	} else {
		for (;i != 0; i = page_sizes_super[i]) {
			if (walk[walk_base[i]].inc_cnt != 0) cos_faa((int*)walk[walk_base[i]].p_glb, -1);
		}
	}

	return ret;
}

int
retypetbl_retype2user(void *pa, u32_t order)
{
	return mod_mem_type(pa, order, RETYPETBL_USER);
}

int
retypetbl_retype2kern(void *pa, u32_t order)
{
	return mod_mem_type(pa, order, RETYPETBL_KERN);
}

/* implemented in pgtbl.c */
int tlb_quiescence_check(u64_t unmap_time);

/* Macro used to change types atomically */
#define TYPE_SWAP_PREPARE(PTR, OLD_TYPE, NEW_TYPE) \
do { \
	old_temp.refcnt_atom.v = (PTR)->refcnt_atom.v; \
	temp.refcnt_atom.v = old_temp.refcnt_atom.v; \
	old_temp.refcnt_atom.type = (OLD_TYPE); \
	temp.refcnt_atom.type = (NEW_TYPE); \
} \
while(0)

/* Retype something back to frame */
int
retypetbl_retype2frame(void *pa, u32_t order)
{
	struct retype_entry_glb *glb_retype_info;
	struct retype_entry     *retype_info;
	struct retype_entry     temp, old_temp;
	union refcnt_atom       local_u;
	u32_t                   old_v, idx;
	u64_t                   last_unmap;
	int                     i, j, cpu, ret, sum;
	mem_type_t              old_type;

	PA_BOUNDARY_CHECK();

	idx = GET_MEM_IDX(pa);
	assert(idx < N_MEM_SETS);

	/* Exact page granularity */
	glb_retype_info = GET_GLB_RETYPE_ENTRY(idx, order);
	old_type        = glb_retype_info->refcnt_atom.type;
	/* Only can untype typed mem sets */
	if (unlikely(old_type != RETYPETBL_USER && old_type != RETYPETBL_KERN)) return -EPERM;

	/* lock down the memory set we are going to untype */
	TYPE_SWAP_PREPARE(glb_retype_info, old_temp.refcnt_atom.type, RETYPETBL_RETYPING);
	ret = retypetbl_cas((u32_t*)glb_retype_info, old_temp.refcnt_atom.v, temp.refcnt_atom.v);
	if (ret != CAS_SUCCESS) return ret;

	for (i = 0; i < NUM_CPU; i++) {
		retype_info = GET_RETYPE_CPU_ENTRY(i, idx, order);
		TYPE_SWAP_PREPARE(retype_info, old_temp.refcnt_atom.type, RETYPETBL_RETYPING);
		/* See if we can successfully lock it */
		if (retypetbl_cas((u32_t*)retype_info, old_temp.refcnt_atom.v, temp.refcnt_atom.v) != CAS_SUCCESS) cos_throw(err, -EINVAL);
	}

	/* Now all of them are frozen. add up the counts to see this still mapped somewhere */
	last_unmap = 0;
	sum = 0;
	for (cpu = 0; cpu < NUM_CPU; cpu++) {
		sum += GET_RETYPE_CPU_ENTRY(cpu, idx, order)->refcnt_atom.ref_cnt;
		/* for tlb quiescence check */
		if (last_unmap < GET_RETYPE_CPU_ENTRY(cpu, idx, order)->last_unmap) {
			last_unmap = GET_RETYPE_CPU_ENTRY(cpu, idx, order)->last_unmap;
		}
	}

	/* Only can retype when there's no more mapping */
	if (sum != 0) cos_throw(err, -EINVAL);

	/* 
	 * Before retype any type of memory back to untyped, we need
	 * to make sure TLB quiescence has been achieved after the
	 * last unmapping of any pages in this memory set
	 */
	if (!tlb_quiescence_check(last_unmap)) cos_throw(err, -EQUIESCENCE);
	cos_mem_fence();

	/* Repetitively add the record of a page set to a type, into the level one above it */
	if (old_type == RETYPETBL_USER) {
		for (j = MAX_PAGE_ORDER; j > (int)order; j = page_sizes_small[j]) {
			glb_retype_info = GET_GLB_RETYPE_ENTRY(idx, j);
			/* The following is atomic with FAA */
			cos_faa((int*)glb_retype_info, -(1 << RETYPE_ENT_GLB_REFCNT_SZ));
		}
	} else {
		for (j = MAX_PAGE_ORDER; j > (int)order; j = page_sizes_small[j]) {
			glb_retype_info = GET_GLB_RETYPE_ENTRY(idx, j);
			/* The following is atomic with FAA */
			cos_faa((int*)glb_retype_info, -1);
		}
	}

	/* Update all the per-cpu variables one by one */
	for (cpu = 0; cpu < NUM_CPU; cpu++) {
		retype_info = GET_RETYPE_CPU_ENTRY(cpu, idx, order);
		retype_info->refcnt_atom.v = 0;
	}

	TYPE_SWAP_PREPARE(glb_retype_info, RETYPETBL_RETYPING, RETYPETBL_UNTYPED);
	ret = retypetbl_cas((u32_t*)glb_retype_info, old_temp.refcnt_atom.v, temp.refcnt_atom.v);
	assert(ret == CAS_SUCCESS);

	return 0;
err:
	/* restore per-core retype entries. (is this necessary?) */
	/* cpu is the one we tried to modify but fail. So we need to
	 * restore [0, cpu-1]. */
	for (i--; i >= 0; i--) {
		retype_info = GET_RETYPE_CPU_ENTRY(i, idx, order);
		TYPE_SWAP_PREPARE(retype_info, old_temp.refcnt_atom.type, old_type);
		retypetbl_cas((u32_t*)retype_info, old_temp.refcnt_atom.v, temp.refcnt_atom.v);
	}
	/* and global entry. */
	{
		int ret_local;
		/* Restore the global entry to old type before return:
		 * only us can change it. */
		TYPE_SWAP_PREPARE(glb_retype_info, RETYPETBL_RETYPING, old_type);
		ret_local = retypetbl_cas((u32_t*)glb_retype_info, old_temp.refcnt_atom.v, temp.refcnt_atom.v);
		assert(ret_local == CAS_SUCCESS);
	}

	return ret;
}

void
retype_tbl_init(void)
{
	int i, j;

	/* Alignment & size checks! */
	assert(sizeof(struct retype_info) % CACHE_LINE == 0);
	assert(sizeof(struct retype_info_glb) % CACHE_LINE == 0);
	assert(sizeof(retype_tbl) % CACHE_LINE == 0);
	assert(sizeof(glb_retype_tbl) % CACHE_LINE == 0);
	assert((int)retype_tbl % CACHE_LINE == 0);
	assert((int)glb_retype_tbl % CACHE_LINE == 0);

	assert(sizeof(union refcnt_atom) == sizeof(u32_t));
	assert(RETYPE_ENT_TYPE_SZ + RETYPE_ENT_REFCNT_SZ == 32);
	assert(CACHE_LINE % sizeof(struct retype_entry) == 0);

	for (i = 0; i < NUM_CPU; i++) {
		for (j = 0; j < N_RETYPE_SLOTS; j++) {
			retype_tbl[i].mem_set[j].refcnt_atom.type    = RETYPETBL_UNTYPED;
			retype_tbl[i].mem_set[j].refcnt_atom.ref_cnt = 0;
			retype_tbl[i].mem_set[j].last_unmap          = 0;
		}
	}

	for (i = 0; i < N_RETYPE_SLOTS; i++) {
		glb_retype_tbl[i].info.refcnt_atom.type = RETYPETBL_UNTYPED;
		glb_retype_tbl[i].info.refcnt_atom.user_cnt = 0;
		glb_retype_tbl[i].info.refcnt_atom.kernel_cnt = 0;
		glb_retype_tbl[i].info.kernel_ref = 0;
	}

	cos_mem_fence();

	return;
}
